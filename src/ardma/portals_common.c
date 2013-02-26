#include "ardma_common.h"
#include "portals_common.h"
#include "../adios_internals.h"
#include "ardma_logger.h"


extern int portal_init_common(ninfo *local_info)
{
	
    //initialize portals device

    int retval = 0;
    int max_interfaces;

    ardma_logger_init(NULL, 3, -1);

#if  defined(__LIBCATAMOUNT__)
    ptl_interface_t ptl_iface = CRAY_QK_NAL;
#elif defined(__CRAYXT_COMPUTE_LINUX_TARGET) || defined(XT3)
    ptl_interface_t ptl_iface = CRAY_UK_SSNAL;
#else
    ptl_interface_t ptl_iface = PTL_NALTYPE_UTCP;
#endif
    ptl_ni_limits_t nilimits;
    ptl_handle_ni_t nihandle;
    ptl_uid_t uid;
    ptl_process_id_t pid;
    ptl_handle_eq_t eq_handle;
    ptl_handle_me_t me_handle;
    ptl_pt_index_t  ptl_index;
    ptl_handle_eq_t eq_handle_data;

    ptl_process_id_t pidany;
    pidany.pid =  PTL_PID_ANY ;
    pidany.nid =  PTL_NID_ANY ;


    retval = PtlInit(&max_interfaces);
    if(retval != PTL_OK)
    {
	log_error("PORTALS: PtlInit \t %s\n", PtlErrorStr(retval));
	return retval;
    }

    log_debug("max interface = %d\n", max_interfaces);
	
	
    retval = PtlNIInit(ptl_iface, PTL_PID_ANY, NULL, &nilimits, &nihandle);
    if(retval != PTL_OK && retval != PTL_IFACE_DUP)
    {
	log_error("PORTALS PtlNIInit \t%s\n", PtlErrorStr(retval));
	return retval;
		
    }
	
    log_debug("%d %d %d %d %d %d %d %d\n",
	      nilimits.max_mes, nilimits.max_mds,
	      nilimits.max_eqs, nilimits.max_ac_index,
	      nilimits.max_pt_index, nilimits.max_md_iovecs,
	      nilimits.max_me_list, nilimits.max_getput_md);
    

    retval = PtlGetUid(nihandle, &uid);
    if (retval != PTL_OK)
    {
        log_error("PORTALS: PtlGetUid \t%s\n", PtlErrorStr(retval));
        return retval;
    }

    retval = PtlGetId(nihandle, &pid);
    if (retval != PTL_OK)
    {
        log_error("PORTALS: PtlGetId \t%s\n", PtlErrorStr(retval));
        return retval;
    }

    retval = PtlEQAlloc(nihandle, LISTSIZE, PTL_EQ_HANDLER_NONE, &eq_handle);
    if(retval != PTL_OK)
    {
	log_error("PORTALS: PtlEQAlloc\t%s\n", PtlErrorStr(retval));
	return retval;
    }
	
    retval = PtlMEAttachAny(nihandle, &ptl_index, pidany, 
			    CONN_MATCH, 0, 
			    PTL_RETAIN, &me_handle);
    if(retval != PTL_OK)
    {
	log_error("PORTALS: PtlMEAttachAny\t%s\n", PtlErrorStr(retval));
	return retval;
    }
	

    log_info("Allocating the memory and MD for connection requests\n");
	
	

    local_info->iface = ptl_iface;
    local_info->nilimits = nilimits;
    local_info->nihandle = nihandle;
    local_info->uid = uid;
    local_info->pid = pid;
    local_info->index = ptl_index;
    local_info->match = CONN_MATCH;//this is static right now but we might
    //have to make this a dynamically selected number if we deal with
    //multiple data types perhaps. we might use some of the match/ignore tricks?
    local_info->eqh = eq_handle;
    local_info->meh = me_handle;
	
	
    log_debug("iface = %d nihandle = %d uid = %d pid.pid = %d pid.nid = %d index = %d meh = %d eqh = %d\n",
	      local_info->iface, local_info->nihandle, local_info->uid,
	      local_info->pid.pid, local_info->pid.nid, local_info->index,
	      local_info->meh, local_info->eqh);

    return 0;
}