#!/bin/bash
#
# Test if the ADIOS query framework can successfully perform a range of *serial* queries on simple, pre-defined datasets
# Parallel query tests are tested in *TODO*
#
# Uses:
# - tests/C/query/common/build_indexed_dataset (executable, produces the pre-defined datasets)
# - tests/C/query/common/xml-testcases/*/* (custom XML files describing interesting queries over the predefined datasets)
#   - e.g. tests/C/query/common/xml-testcases/DS1/simple-query.xml
# - tests/C/query/common/compute_expected_query_results (executable, runs queries via sequential scan over a BP file)
#
# Environment variables set by caller:
# MPIRUN        Run command
# NP_MPIRUN     Run commands option to set number of processes
# MAXPROCS      Max number of processes allowed
# HAVE_FORTRAN  yes or no
# SRCDIR        Test source dir (.. of this script)
# TRUNKDIR      ADIOS trunk dir

# This function prints a message to stderr, then exits with the
# given error code (defaults to 1). Usage: die <msg> <code>
function die() {
  EC=${2-1}
  echo "$1" >&2
  exit $EC
}

# mpirun for serial command
MPIRUN_SERIAL="$MPIRUN $NP_MPIRUN 1 $EXEOPT"

# Basic directory structure
QUERY_TEST_DIR="$TRUNKDIR/tests/C/query"
QUERY_COMMON_DIR="$QUERY_TEST_DIR/common"

# Some external tools to use
DATASET_BUILDER_EXE_BASENAME="build_indexed_dataset"
QUERY_SEQSCAN_EXE_BASENAME="compute_expected_query_results"
QUERY_EXE_BASENAME="adios_query_test"

DATASET_BUILDER_EXE_PATH="$QUERY_COMMON_DIR/$DATASET_BUILDER_EXE_BASENAME"
QUERY_SEQSCAN_EXE_PATH="$QUERY_COMMON_DIR/$QUERY_SEQSCAN_EXE_BASENAME"
QUERY_EXE_PATH="$QUERY_COMMON_DIR/$QUERY_EXE_BASENAME"

# Check for the executability of all executables that we need
[ -f "$DATASET_BUILDER_EXE_PATH" -a -x "$DATASET_BUILDER_EXE_PATH" ] || die "ERROR: $DATASET_BUILDER_EXE_PATH is not executable"
[ -f "$QUERY_SEQSCAN_EXE_PATH"   -a -x "$QUERY_SEQSCAN_EXE_PATH"   ] || die "ERROR: $QUERY_SEQSCAN_EXE_PATH is not executable"
[ -f "$QUERY_EXE_PATH"           -a -x "$QUERY_EXE_PATH"           ] || die "ERROR: $QUERY_EXE_PATH is not executable"

# Copy the external tools to the working directory for convenience
DATASET_BUILDER_EXE_LOCAL="./$DATASET_BUILDER_EXE_BASENAME"
QUERY_SEQSCAN_EXE_LOCAL="./$QUERY_SEQSCAN_EXE_BASENAME"
QUERY_EXE_LOCAL="./$QUERY_EXE_BASENAME"
cp $DATASET_BUILDER_EXE_PATH $DATASET_BUILDER_EXE_LOCAL
cp $QUERY_SEQSCAN_EXE_PATH   $QUERY_SEQSCAN_EXE_LOCAL
cp $QUERY_EXE_PATH           $QUERY_EXE_LOCAL

# Check for the "directory"-ness of the query XML dir
QUERY_XML_DIR="$QUERY_TEST_DIR/query-xmls/"
[ -d "$QUERY_XML_DIR" ] || die "ERROR: $QUERY_XML_DIR is not a directory"

# All pre-defined dataset IDs (which can be extracted from build_indexed_dataset)
ALL_DATASET_IDS="DS1 DS2 DS3 DS-particle"

# Check that query XML subdirectories exist for all datasets we're testing 
for DSID in $ALL_DATASET_IDS; do
  [ -d "$QUERY_XML_DIR/$DSID" ] ||
    die "ERROR: $QUERY_XML_DIR/$DSID is not a directory"
done

# All query engine implementations to test
# TODO: Expand to FastBit once we understand it properly
# TODO: Detect which query engines are built with this install of ADIOS, and only test those
ALL_QUERY_ENGINES="alacrity"



function init_work_directory() {
  echo "STEP 1: INITIALIZING THE TEST WORKING DIRECTORY..."
  
  local QUERY_ENGINE
  
  # Set up subdirectories for each query engine, since each
  # one will need its own BP file indexed in a particular way
  for QUERY_ENGINE in $ALL_QUERY_ENGINES; do
    mkdir -p ./$QUERY_ENGINE
  done
}



function invoke_dataset_builder() {
  local DSID="$1"
  local DSOUTPUT="$2"
  local TRANSFORM_ARG="$3"
  [[ $# -eq 3 ]] || die "ERROR: Internal testing error, invalid parameters to invoke_dataset_builder: $@"
  
  set -o xtrace
  $DATASET_BUILDER_EXE_LOCAL "$DSID" "$DSOUTPUT" "$TRANSFORM_ARG" ||
    die "ERROR: $DATASET_BUILDER_EXE_LOCAL failed with exit code $? (on dataset $DSID, outputting to $DSOUTPUT, using transform argument $TRANSFORM_ARG)"
  set +o xtrace
  
  # Rename the ADIOS XML used for writing, so it's more clear what it is used
  # for (i.e., not a query test specification XML)
  mv "$DSOUTPUT.xml" "$DSOUTPUT.create.xml"
  
  # Ensure the dataset was actually produced
  local INDEXED_DS="$DSOUTPUT.bp"
  [ -f "$INDEXED_DS" ] ||
    die "ERROR: $DATASET_BUILDER_EXE_LOCAL did not produce expected output BP file \"$INDEXED_DS\""
    
  #echo $INDEXED_DS  # Return the BP filename
}

function build_indexed_datasets_alacrity() {
  local DSID="$1"
  local DSOUTPUT="$2"
  [[ $# -eq 2 ]] || die "ERROR: Internal testing error, invalid parameters to build_indexed_datasets_alacrity: $@"
  
  invoke_dataset_builder "$DSID" "$DSOUTPUT.ii" "alacrity:indexForm=ALInvertedIndex"
#  invoke_dataset_builder "$DSID" "$DSOUTPUT.cii" "alacrity:indexForm=ALCompressedInvertedIndex"
}

function build_datasets() {
  echo "STEP 2: INDEXING ALL TEST DATASETS USING ALL ENABLED INDEXING METHODS"
  echo "(ALSO PRODUCING A NON-INDEXED VERSION OF EACH DATASET FOR REFERENCE)"

  local DSID
  local QUERY_ENGINE
  
  for DSID in $ALL_DATASET_IDS; do
    # Build a no-indexed dataset for use by the sequential
    # scan oracle
    invoke_dataset_builder "$DSID" "$DSID.noindex" "none"
    
    # Build indexed versions of this dataset for all query engines
    for QUERY_ENGINE in $ALL_QUERY_ENGINES; do
      echo "Producing indexed versions of $DSID for query engine $QUERY_ENGINE"
      local QE_WORKDIR="./$QUERY_ENGINE"

      # Index the dataset using any indexing configurations desired for this
      # particular query engine datasets. This function should produce BP files
      # of the form $QE_WORKDIR/$DSID.$QUERY_ENGINE.<indexing config name>.bp
      # (e.g. .../DS1.alacrity.cii.bp, .../DS1.fastbit.precision2.bp
      build_indexed_datasets_$QUERY_ENGINE "$DSID" "$QE_WORKDIR/$DSID.$QUERY_ENGINE"
    done
  done
}


# STEP 3: RUN ALL QUERIES USING ALL QUERY ENGINES
function query_datasets() {
  echo "STEP 3: RUNNING ALL QUERIES USING ALL ENABLED QUERY ENGINES ON THE INDEXED DATASETS"

  local DSID
  local QUERY_XML
  local QUERY_ENGINE
  local INDEXED_DS
  
  for DSID in $ALL_DATASET_IDS; do
    local NOINDEX_DS="$DSID.noindex.bp"
  
    # Iterate over all pre-defined queries
    for QUERY_XML in "$QUERY_XML_DIR/$DSID"/*.xml; do
      local QUERY_XML_BASENAME="${QUERY_XML##*/}"     # Strip the path
      local QUERY_NAME="${QUERY_XML_BASENAME%%.xml}"  # Strip the .xml suffix
      
      # Copy the query XML into our working directory for easy access
      local QUERY_XML_LOCAL="${QUERY_XML_BASENAME}"
      cp "$QUERY_XML" "$QUERY_XML_LOCAL" 
      
      # Compute the expected results
      local EXPECTED_POINTS_FILE="$QUERY_NAME.expected-points.txt"
      set -o xtrace
      $MPIRUN_SERIAL "$QUERY_SEQSCAN_EXE_LOCAL" "$NOINDEX_DS" "$QUERY_XML" > "$EXPECTED_POINTS_FILE" ||
        die "ERROR: $QUERY_SEQSCAN_EXE_LOCAL failed with exit code $?"
      set +o xtrace
      
      # NOTE: the sequential scan program produces a point list that is guaranteed to be sorted in C array order, so no need to sort it here

      # Run the query for each query engine implementation and compare to the expected results
      for QUERY_ENGINE in $ALL_QUERY_ENGINES; do
        local QE_WORKDIR="./$QUERY_ENGINE"

        for INDEXED_DS in "$QE_WORKDIR/$DSID.$QUERY_ENGINE".*.bp; do
          INDEXING_NAME=${INDEXED_DS##*/$DSID.$QUERY_ENGINE.}
          INDEXING_NAME=${INDEXING_NAME%.bp}

          local OUTPUT_POINTS_FILE="$QE_WORKDIR/$QUERY_NAME.$QUERY_ENGINE-$INDEXING_NAME-points.txt"

          # Run the query through ADIOS Query to get actual results
          echo
          echo "====== RUNNING TEST $QUERY_NAME USING QUERY ENGINE $QUERY_ENGINE ON DATASET $INDEXED_DS ======"
          echo
          set -o xtrace
          $MPIRUN_SERIAL "$QUERY_EXE_LOCAL" "$INDEXED_DS" "$QUERY_XML_LOCAL" "$QUERY_ENGINE"  > "$OUTPUT_POINTS_FILE" ||
            die "ERROR: $QUERY_EXE_LOCAL failed with exit code $?"
          set +o xtrace

          # Sort the output points in C array order, since the query engine makes no guarantee as to the ordering of the results
          # Sort file in place (-o FILE) with numerical sort order (-n) on each of the first 9 fields (-k1,1 ...)
          # Assumes the output will have at most 8 dimensions (+ 1 timestep column == 9), add more if needed (or a generalized column counter)
          sort -n -k1,1 -k2,2 -k3,3 -k4,4 -k5,5 -k6,6 -k7,7 -k8,8 -k9,9 \
            "$OUTPUT_POINTS_FILE" -o "$OUTPUT_POINTS_FILE"

          # Compare the actual and expected results via diff (the matching points are sorted lexicographically
          if ! diff -q "$EXPECTED_POINTS_FILE" "$OUTPUT_POINTS_FILE"; then
            echo "ERROR: ADIOS Query does not return the expected points matching query $QUERY_NAME on dataset $DSID using query engine $QUERY_ENGINE"
            echo "Compare \"$EXPECTED_POINTS_FILE\" (expected points) vs. \"$OUTPUT_POINTS_FILE\" (returned points) in \"$PWD\""
            echo "The BP file queried is \"$INDEXED_DS\" and the query is specified by \"$QUERY_XML_LOCAL\""
            exit 1  
          fi
        done
      done
    done
  done
}

# FINALLY, CALL THE FUNCTIONS IN SEQUENCE
init_work_directory
build_datasets
query_datasets
