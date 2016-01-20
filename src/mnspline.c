/* mnspline.c
 * Natural cubic spline interpolation
 * 
 * based on 
 * Numerical Recipes in C: The Art of Scientific Computing
 *
 * With additions: (January 2016)
 *  performs interpolation on an array
 *  caches the previous result to avoid bisecting on every iteration
 *  interpolates array in parallell with threads (OpenMP) 
 */

#include <stdlib.h>
#include <omp.h>

static inline void bisect(const double *pxa, const double x, int *klo, int *khi);

//#define DEBUG 1

/* spline - calculate the 2nd. derivatives of the interpolating function
 * only called once - gives input to splint
 * returns -1 on malloc failure
 */
int spline(const double *px,      // Array of function evaluation points, with x[1] < x[2] < ... < x[n]
                const double *py, // Array of function evaluated at above points
                const int n,      // Size of array px and py
                double *py2       // Ptr to array, returns the 2nd derivatives of the interpolating function
                )
{
        double qn = 0.0;
        double un = 0.0;
        double p;
        double sig;
        double *pu;

        if ( (pu = (double*) malloc((n-1) * sizeof(double))) == NULL )
                return -1;

        py2[0] = 0.0;
        pu[0] = 0.0;

        for (int i = 1; i < n - 1; i++)
        {
                sig     = (px[i] - px[i-1]) / (px[i+1] - px[i-1]);
                p       = sig * py2[i-1] + 2.0;
                py2[i]  = (sig - 1.0) / p;
                pu[i]   = (py[i+1] - py[i]) / (px[i+1] - px[i]) - 
                          (py[i] - py[i-1]) / (px[i] - px[i-1]);
                pu[i]   = (6.0 * pu[i] / (px[i+1] - px[i-1]) -
                          sig * pu[i-1]) / p; 
        }
        
        py2[n-1] = (un - qn * pu[n-2]) / (qn * py2[n-2] + 1.0);

        for (int k = n - 2; k >= 0; k--)
                py2[k] = py2[k] * py2[k+1] + pu[k];

        free(pu);
        return 0;
}

/* splint - perform the interpolation
 * returns 0 - currently does no error checking (do this in wrapper)
 */
int splint(const double *pxa,       // Same input as to spline: px
                const double *pya,  // Same input as to spline: py
                const double *py2a, // The output array from spline: py2
                const int n,        // Same input as to spline: n
                const double* px,   // An array of function evaluation points to be interpolated
                double *py,         // Ptr to array, returns the interpolated function values evaluated at px
                const int nx        // size of array px and py
                )
{
        int klo = 0;
        int khi = n - 1;
        int pklo = 0;
        int pkhi = 1;
        double h;
        double b;
        double a;

        #pragma omp parallel for \
        firstprivate(klo, khi, pklo, pkhi) \
        private(h, b, a)
        for (int i = 0; i < nx; i++)
        {
                /*printf("tid: %d i: %d \n", omp_get_thread_num(), i);*/
                if ( (pxa[pklo] <= px[i]) && (pxa[pkhi] > px[i]) ) 
                {
                         //printf("caching i: %d x %f pklo %d pkhi %d klo %d khi %d\n",
                         //           i, px[i], pklo, pkhi, klo, khi);
                        klo = pklo;
                        khi = pkhi;
                }
                else
                { 
                        bisect(pxa, px[i], &klo, &khi);
                        pklo = klo;
                        pkhi = khi;
                }
                h     = pxa[khi] - pxa[klo];
                a     = (pxa[khi] - px[i]) / h;
                b     = (px[i] - pxa[klo]) / h;
                py[i] = a * pya[klo] + b * pya[khi] + 
                    ((a*a*a -a) * py2a[klo] + (b*b*b - b) * py2a[khi]) * (h*h) / 6.0;

                klo = 0;
                khi = n - 1;
        }

        return 0;
}

static inline void bisect(const double *pxa, const double x, int *klo, int *khi)
{
        int mid;
        while (*khi - *klo > 1)
        {
                mid = *klo + ((*khi - *klo) >> 1); 
                if (pxa[mid] > x)
                        *khi = mid;
                else
                        *klo = mid;
        }
}

#ifdef DEBUG
#include <stdio.h>
int main() {

        double x[10]           = {1.0, 2.0, 3.0, 4.0, 5.0,
                                  6.0, 7.0, 8.0, 9.0, 10.0};
        double y[10]          = {0.841471, 0.909297, 0.14112,
                                -0.756802, -0.958924, -0.279415,
                                 0.656987, 0.989358, 0.412118, 
                                -0.544021};
        double y2desired[10]  = {0.0, -1.23211, -0.087569,
                                 0.803919, 1.0467, 0.299074, 
                                -0.701635, -1.11672, -0.289171,
                                 0.0};
        double yintdesired[10] = {0.952391, 0.607689 , -0.352613, 
                                 -0.973527, -0.703281 , 0.213946 ,
                                  0.936819, 0.788606, -0.0478781,
                                  -0.34354};
        double xint[10]        = {1.5, 2.5, 3.5, 4.5, 5.5,
                                  6.5, 7.5, 8.5, 9.5, 9.8};

        double y2[10];
        double yint[10];

        spline(&x[0], &y[0], 10, &y2[0]);

        splint(&x[0], &y[0], &y2[0], 10,
                        &xint[0], &yint[0], 10);

        
        printf("y2\ty2(tst)\tyint\tyint(tst)\n");
        for (int i=0; i < 10; i++) 
                printf("%.3f\t%.3f\t%.3f\t%.3f\n",
                                y2[i], y2desired[i], yint[i], yintdesired[i]);
}
#endif

