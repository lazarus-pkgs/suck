#include <config.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include <string.h>

#include "suck_config.h"
#include "suck.h"
#include "suckutils.h"

#include "ssort.h"

/* THREE-WAY RADIX QUICKSORT, faster version */
/* From Dr Dobb's Journal Nov 1999 */
/* modified to work with PList vice strings */

/* prototypes */
void swap(PList *, int, int);
void vecswap(PList *, int, int, int);
int med3func(PList *, int, int, int, int);
void inssort(PList *, int, int);

/* Support functions */

#ifndef min
#define min(a, b) ((a)<=(b) ? (a) : (b))
#endif

/*-------------------------------------------------------------*/
void swap(PList *a, int i, int j) {
	PList t = a[i];
	a[i] = a[j];
	a[j] = t;
}
/*--------------------------------------------------------------*/
void vecswap(PList *a, int i, int j, int n) {
	while (n-- > 0) {
		swap(a, i++, j++);
	}
}
/*---------------------------------------------------------------*/
int med3func(PList *a, int ia, int ib, int ic, int depth) {
	int va, vb, vc;
	if ((va=a[ia]->msgnr[depth]) == (vb=a[ib]->msgnr[depth])) {
		return ia;
	}
	if ((vc=a[ic]->msgnr[depth]) == va || vc == vb) {
		return ic;
	}
	return va < vb ?
		(vb < vc ? ib : (va < vc ? ic : ia ) )
		: (vb > vc ? ib : (va < vc ? ia : ic ) );
}
/*-----------------------------------------------------------------*/
void inssort(PList *a, int n, int depth) {
	int i, j;
	for (i = 1; i < n; i++) {
		for (j = i; j > 0; j--) {
			if (strcmp(&(a[j-1]->msgnr[depth]), &(a[j]->msgnr[depth])) <= 0) {
				break;
			}
			swap(a, j, j-1);
		}
	}
}
/*-----------------------------------------------------------------*/
void ssort(PList *a, int n, int depth) {
	int le, lt, gt, ge, r, v;
	int pl, pm, pn, d;
	if (n <= 10) {
		inssort(a, n, depth);
		return;
	}
	pl = 0;
	pm = n/2;
	pn = n-1;
	if (n > 50) {
		d = n/8;
		pl = med3func(a, pl, pl+d, pl+2*d,depth);
		pm = med3func(a, pm-d, pm, pm+d,depth);
		pn = med3func(a, pn-2*d, pn-d, pn,depth);
	}
	pm = med3func(a, pl, pm, pn,depth);
	swap(a, 0, pm);
	v = a[0]->msgnr[depth];
	for (le = 1; le < n && a[le]->msgnr[depth] == v; le++) {
		;
	}
	if (le == n) {
		if (v != 0) {
			ssort(a, n, depth+1);
		}
		return;
	}
	lt = le;
	gt = ge = n-1;
	for (;;) {
		for ( ; lt <= gt && a[lt]->msgnr[depth] <= v; lt++) {
			if (a[lt]->msgnr[depth] == v) {
				swap(a, le++, lt);
			}
		}
		for ( ; lt <= gt && a[gt]->msgnr[depth] >= v; gt--) {
			if (a[gt]->msgnr[depth] == v) {
				swap(a, gt, ge--);
			}
		}
		if (lt > gt) {
			break;
		}
		swap(a, lt++, gt--);
	}
	r = min(le, lt-le);
	vecswap(a, 0, lt-r, r);
	r = min(ge-gt, n-ge-1);
	vecswap(a, lt, n-r, r);
	ssort(a, lt-le, depth);
	if (v != 0) {
		ssort(a + lt-le, le + n-ge-1, depth+1);
	}
	ssort(a + n-(ge-gt), ge-gt, depth);
}
/*-------------------------------------------------------------------------------*/
PList my_bsearch(PList *arr, char *matchnr, int nrin) {

	int val, my_index, left = 0, right = nrin ;

	PList retval = NULL;

	while( left < right ) {
		my_index = ( right + left) / 2;  /* find halfway pt */

		val = qcmp_msgid(matchnr, arr[my_index]->msgnr);

		if(val < 0) {
			right = my_index ;
		}
		else if(val > 0) {
			left = my_index + 1;
		}
		else {
			retval = arr[my_index];
			break;
		}

	}

	return retval;
}

