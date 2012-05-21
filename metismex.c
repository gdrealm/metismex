/****************************************************************************
* metismex.c
* Public domain MATLAB CMEX-file to let you use METIS-4.0 from MATLAB.
* Usage:
* [part,edgecut] = metismex('PartGraphRecursive',A,nparts,wgtflag,options)
* [part,edgecut] = metismex('PartGraphKway',A,nparts,wgtflag,options)
* [perm,iperm] = metismex('EdgeND',A,options)
* [perm,iperm] = metismex('NodeND',A,options)
* sep = metismex('NodeBisect',A,wgtflag,options)
*
* Output arguments, along with the wgtflag and options input arguments,
* are optional. See the METIS manual for the meanings of the arguments.
*
* Note that error checking is not done: make sure A is structurally
* symmetric or it will crash.
*
* To compile, you need to have Metis 5, and do something like (OSX)
  mex -O -largeArrayDims -I../include -I../libmetis -I../GKlib -L../build/Darwin-x86_64/libmetis -lmetis metismex.c -D__thread= -DLINUX
* If you get unreferenced symbol errors __get_tls_addr, for instance, then
* I found:
   mex LDFLAGS='-pthread -shared -Wl,--version-script,\$TMW_ROOT/extern/lib/\$Arch/\$MAPFILE' ...
     -O -largeArrayDims -I../include -I../libmetis -I../GKlib/trunk ...
     -L../build/Linux-x86_64/ -lmetis metismex.c -DLINUX -DUNIX
*
* Robert Bridson (and David F. Gleich)
*****************************************************************************/

/**
 * Modifications by David Gleich, started 2008-10-20
 * Fixed 64-bit Matlab sparse issues 
 */

#include "mex.h"

#if MX_API_VER < 0x07030000
typedef int mwIndex;
typedef int mwSize;
#endif /* MX_API_VER */

#include <strings.h>


/* 
We cannot change the typewidth from within matlab because
it depends on how metis was compiled, we can just throw an error
if it was compiled incorrectly.
#ifdef MX_COMPAT_32
#define idx_tWIDTH 32
#else
#define idx_tWIDTH 64
#endif
*/
/* MX_COMPAT_32 */

#include <metislib.h>

/*************************************************************************
* Given a graph, find a node separator bisecting it (roughly). The number
* of bisector nodes is returned in *nbnd, and the nodes themselves in the
* array bnds, which should already be allocated to have enough room.
**************************************************************************/
#if 0
void METIS_NodeBisect(idx_t nvtxs, idx_t *xadj, idx_t *adjncy,
                      idx_t *vwgt, idx_t *adjwgt, idx_t wgtflag,
                      idx_t *options, idx_t *nbnd, idx_t *bnds, double unbalance)
{
  idx_t i, j, tvwgt, tpwgts2[2];
  GraphType graph;
  CtrlType ctrl;
  idx_t *label, *bndind;

  if (options[0] == 0) {  /* Use the default parameters */
    ctrl.CType   = ONMETIS_CTYPE;
    ctrl.IType   = ONMETIS_ITYPE;
    ctrl.RType   = ONMETIS_RTYPE;
    ctrl.dbglvl  = ONMETIS_DBGLVL;
  }
  else {
    ctrl.CType   = options[OPTION_CTYPE];
    ctrl.IType   = options[OPTION_ITYPE];
    ctrl.RType   = options[OPTION_RTYPE];
    ctrl.dbglvl  = options[OPTION_DBGLVL];
  }
  ctrl.nseps = 5;    /* Take the best of 5 separators */
  ctrl.optype = OP_ONMETIS;
  ctrl.CoarsenTo = 50;

  IFSET(ctrl.dbglvl, DBG_TIME, InitTimers(&ctrl));
  IFSET(ctrl.dbglvl, DBG_TIME, starttimer(ctrl.TotalTmr));

  SetUpGraph(&graph, OP_ONMETIS, nvtxs, 1, xadj,adjncy,vwgt,adjwgt,wgtflag);

  /* Determine the weights of the partitions */
  tvwgt = idxsum(nvtxs, graph.vwgt);
  tpwgts2[0] = tvwgt/2;
  tpwgts2[1] = tvwgt-tpwgts2[0];

  ctrl.maxvwgt = (1.5*tvwgt)/ctrl.CoarsenTo;

  InitRandom(-1);

  AllocateWorkSpace(&ctrl, &graph, 2);

  MlevelNodeBisectionMultiple (&ctrl, &graph, tpwgts2, unbalance);

  IFSET(ctrl.dbglvl, DBG_SEPINFO, printf("Nvtxs: %6d, [%6d %6d %6d]\n", graph.nvtxs, graph.pwgts[0], graph.pwgts[1], graph.pwgts[2]));

  /* Now indicate the vertex separator */
  *nbnd = graph.nbnd;
  bndind = graph.bndind;
  label = graph.label;
  for (i = 0; i < *nbnd; ++i) 
    bnds[i] = label[bndind[i]];

  IFSET(ctrl.dbglvl, DBG_TIME, stoptimer(ctrl.TotalTmr));
  IFSET(ctrl.dbglvl, DBG_TIME, PrintTimers(&ctrl));

  FreeWorkSpace(&ctrl, &graph);
}
#endif

void convertMatrix (const mxArray *A, idx_t **xadj, idx_t **adjncy,
                    idx_t **vwgt, idx_t **adjwgt)
{
    mwIndex i, j, jbar, n, nnz, *jc, *ir;
    double *pr;

    /* Find MATLAB's matrix structure */
    n = mxGetN(A);
    jc = mxGetJc(A);
    nnz = jc[n];
    ir = mxGetIr(A);
    pr = mxGetPr(A);

    /* Allocate room for METIS's structure */
    *xadj = (idx_t*) mxCalloc (n+1, sizeof(idx_t));
    *adjncy = (idx_t*) mxCalloc (nnz, sizeof(idx_t));
    *vwgt = (idx_t*) mxCalloc (n, sizeof(idx_t));
    *adjwgt = (idx_t*) mxCalloc (nnz, sizeof(idx_t));

    /* Scan the matrix, not copying diagonal terms, and rounding doubles
     * to integer weights */
    (*xadj)[0] = 0;
    jbar = 0;
    for (i = 1; i <= n; i++) {
        for (j = jc[i-1]; j < jc[i]; j++) {
            if (ir[j] != i-1) {
                (*adjncy)[jbar] = ir[j];
                (*adjwgt)[jbar] = (idx_t) pr[j];
                jbar++;
            } else {
                (*vwgt)[i-1] = (idx_t) pr[j];
            }
        }
        (*xadj)[i] = jbar;
    }
}


#define FUNC_IN (prhs[0])
#define A_IN (prhs[1])
#define NPARTS_IN (prhs[2])
#define WGTFLAG_IN (prhs[3])
#define PARTOPTS_IN (prhs[4])
#define NDOPTS_IN (prhs[2])
#define NBWGTFLAG_IN (prhs[2])
#define NBOPTS_IN (prhs[3])

#define SEED_IN (prhs[5])

#define PART_OUT (plhs[0])
#define EDGECUT_OUT (plhs[1])
#define PERM_OUT (plhs[0])
#define IPERM_OUT (plhs[1])
#define SEP_OUT (plhs[0])



#define FUNCNAMELEN 25

/****************************************************************************
* mexFunction: gateway routine for MATLAB interface.
*****************************************************************************/
void mexFunction (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    idx_t i, n, nparts, wgtflag, options[METIS_NOPTIONS] = {0}, edgecut, sepsize;
    idx_t *xadj, *adjncy, *vwgt, *adjwgt, *part, *perm, *iperm, *sep;
    char funcname[FUNCNAMELEN];
    double *optarray, *partpr, *permpr, *ipermpr, *seppr;

    /* First do some general argument checking */
    if (nrhs < 2 || nrhs > 6 || nlhs > 2) {
        mexErrMsgTxt ("Wrong # of arguments");
    }
    if (!mxIsChar(FUNC_IN)) {
        mexErrMsgTxt ("First parameter must be a string");
    }
    n = mxGetN(A_IN);
    if (!mxIsSparse(A_IN) || n!=mxGetM(A_IN)) {
        mexErrMsgTxt ("Second parameter must be a symmetric sparse matrix");
    }
    
    idx_t seed = -1;
    if (nrhs > 5) {
        seed = (idx_t)mxGetScalar(SEED_IN);
    }
    
    InitRandom(seed);
    
    METIS_SetDefaultOptions(options);
    options[METIS_OPTION_SEED] = seed;

    /* Copy the matrix over, getting rid of diagonal, and converting to
     * integer weights */
    convertMatrix (A_IN, &xadj, &adjncy, &vwgt, &adjwgt);

    /* Now figure out which function we have to do */
    mxGetString (FUNC_IN, funcname, FUNCNAMELEN);

    if (strcasecmp(funcname,"PartGraphRecursive")==0) {

        /* Figure out values for nparts, wgtflag and options */
        if (nrhs < 3) {
            mexErrMsgTxt ("Third parameter needed: nparts");
        }
        nparts = (idx_t) mxGetScalar (NPARTS_IN);
        if (nrhs >= 4) {
            wgtflag = (idx_t) mxGetScalar (WGTFLAG_IN);
        } else {
            wgtflag = 0;
        }
        if (wgtflag == 0) {
            for (i=0; i<n; ++i) {
                vwgt[i] = 1;
            }
        }
        
        if (nrhs >= 5) {
            optarray = mxGetPr (PARTOPTS_IN);
            for (i = 1; i < 4; ++i) {
                options[i] = (idx_t) optarray[i-1];
            }
        }

        if (nparts < 2) { 
            mexErrMsgTxt("nparts must be at least 2");
        }

        /* Allocate memory for result of call */
        part = (idx_t*) mxCalloc (n, sizeof(idx_t));
        
        idx_t ncon = 1;
        idx_t* vsize = (idx_t*)mxMalloc(sizeof(idx_t)*n);
        for (i=0; i<n; ++i) { 
            vsize[i] = 1;
        }

        /* Do the call */
        int rval = METIS_PartGraphRecursive (&n, &ncon, xadj, adjncy, vwgt, vsize, adjwgt, 
                &nparts, NULL, NULL, options, &edgecut, part);
        mexPrintf("metis returned: %i\n", rval);

        /* Figure out output values */
        if (nlhs >= 1) {
            PART_OUT = mxCreateDoubleMatrix (1, n, mxREAL);
            partpr = mxGetPr (PART_OUT);
            for (i = 0; i < n; i++) {
                partpr[i] = (double) part[i];
            }

            if (nlhs >= 2) {
                EDGECUT_OUT = mxCreateDoubleMatrix (1, 1, mxREAL);
                mxGetPr(EDGECUT_OUT)[0] = (double) edgecut;
            }
        }

    } else if (strcasecmp(funcname,"PartGraphKway")==0) {

        /* Figure out values for nparts, wgtflag and options */
        if (nrhs < 3) {
            mexErrMsgTxt ("Third parameter needed: nparts");
        }
        nparts = (idx_t) mxGetScalar (NPARTS_IN);
        if (nrhs >= 4) {
            wgtflag = (idx_t) mxGetScalar (WGTFLAG_IN);
        } else {
            wgtflag = 0;
        }
        if (nrhs >= 5) {
            optarray = mxGetPr (PARTOPTS_IN);
            for (i = 1; i < 4; ++i) {
                options[i] = (idx_t) optarray[i-1];
            }
        }
        
        if (nparts < 2) { 
            mexErrMsgTxt("nparts must be at least 2");
        }

        /* Allocate memory for result of call */
        part = (idx_t*) mxCalloc (n, sizeof(idx_t));
        
        idx_t ncon = 1;

        /* Do the call */
        METIS_PartGraphKway (&n, &ncon, xadj, adjncy, vwgt, NULL, adjwgt, 
                &nparts, NULL, NULL, options, &edgecut, part);

        /* Figure out output values */
        if (nlhs >= 1) {
            PART_OUT = mxCreateDoubleMatrix (1, n, mxREAL);
            partpr = mxGetPr (PART_OUT);
            for (i = 0; i < n; i++) {
                partpr[i] = (double) part[i];
            }

            if (nlhs >= 2) {
                EDGECUT_OUT = mxCreateDoubleMatrix (1, 1, mxREAL);
                mxGetPr(EDGECUT_OUT)[0] = (double) edgecut;
            }
        }

    } else if (strcasecmp(funcname,"EdgeND")==0) {

        /* Figure out values for options */
        if (nrhs >= 3) {
            optarray = mxGetPr (NDOPTS_IN);
            for (i = 1; i < 4; ++i) {
                options[i] = (idx_t) optarray[i-1];
            }
        }

        /* Allocate memory for result of call */
        perm = (idx_t*) mxCalloc (n, sizeof(idx_t));
        iperm = (idx_t*) mxCalloc (n, sizeof(idx_t));

        /* Do the call */
        METIS_NodeND (&n, xadj, adjncy, NULL, options, perm, iperm);

        /* Figure out output values */
        if (nlhs >= 1) {
            PERM_OUT = mxCreateDoubleMatrix (1, n, mxREAL);
            permpr = mxGetPr (PERM_OUT);
            for (i = 0; i < n; i++) {
                permpr[i] = perm[i]+1.0;
            }

            if (nlhs >= 2) {
                IPERM_OUT = mxCreateDoubleMatrix (1, n, mxREAL);
                ipermpr = mxGetPr (IPERM_OUT);
                for (i = 0; i < n; i++) {
                    ipermpr[i] = iperm[i]+1.0;
                }
            }
        }

    } else if (strcasecmp(funcname,"NodeND")==0) {

        /* Figure out values for options */
        if (nrhs >= 3) {
            optarray = mxGetPr (NDOPTS_IN);
            for (i = 1; i < 4; ++i) {
                options[i] = (idx_t) optarray[i-1];
            }
            for (i = 5; i < 8; ++i) {
                options[i] = (idx_t) optarray[i-2];
            }
        }

        /* Allocate memory for result of call */
        perm = (idx_t*) mxCalloc (n, sizeof(idx_t));
        iperm = (idx_t*) mxCalloc (n, sizeof(idx_t));

        /* Do the call */
        METIS_NodeND (&n, xadj, adjncy, NULL, options, perm, iperm);

        /* Figure out output values */
        if (nlhs >= 1) {
            PERM_OUT = mxCreateDoubleMatrix (1, n, mxREAL);
            permpr = mxGetPr (PERM_OUT);
            for (i = 0; i < n; i++) {
                permpr[i] = perm[i]+1.0;
            }

            if (nlhs >= 2) {
                IPERM_OUT = mxCreateDoubleMatrix (1, n, mxREAL);
                ipermpr = mxGetPr (IPERM_OUT);
                for (i = 0; i < n; i++) {
                    ipermpr[i] = iperm[i]+1.0;
                }
            }
        }
#if 0
    } else if (strcasecmp(funcname,"NodeBisect")==0) {

        if (nrhs >= 3) {
            wgtflag = (idx_t) mxGetScalar (NBWGTFLAG_IN);
        } else {
            wgtflag = 0;
        }
        if (nrhs >= 4) {
            optarray = mxGetPr (NBOPTS_IN);
            for (i = 1; i < 4; ++i) {
                options[i] = (idx_t) optarray[i-1];
            }
        }

        /* Allocate memory for result of call */
        sep = (idx_t*) mxCalloc (n, sizeof(idx_t));

        /* Do the call */
        METIS_NodeBisect (n, xadj, adjncy, vwgt, adjwgt, wgtflag,
                          options, &sepsize, sep, 1.5);

        /* Figure out output values */
        if (nlhs >= 1) {
            SEP_OUT = mxCreateDoubleMatrix (1, sepsize, mxREAL);
            seppr = mxGetPr (PART_OUT);
            for (i = 0; i < sepsize; i++) {
                seppr[i] = (double) (sep[i]+1);
            }
        }
#endif    
    } else {
        mexErrMsgTxt ("Unknown metismex function");
    }
}

