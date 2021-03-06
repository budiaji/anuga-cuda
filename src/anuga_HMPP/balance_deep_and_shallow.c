#include "hmpp_fun.h"


#ifdef USING_LOCAL_DIRECTIVES
#pragma hmpp balance codelet, target=CUDA args[*].transfer=atcall
#endif
void balance_deep_and_shallow(
        int N,
        int N3,
        double H0,
        double alpha_balance,
        int tight_slope_limiters,
        int use_centroid_velocities,

        double wc[N],   // stage_centroid_values
        double zc[N],   // elevation_centroid_values
        double wv[N3],  // stage_vertex_values
        double zv[N3],  // elevation_vertex_values
        //double* hvbar,// Retire this
        double xmomc[N],  // xmom_centroid_values
        double ymomc[N],  // ymom_centroid_values
        double xmomv[N3],  // xmom_vertex_values
        double ymomv[N3]   // ymom_vertex_values
        ) 
{
    int k, k3, i;

    double dz, hmin, alpha, h_diff, hc_k;
    double epsilon = 1.0e-6; // FIXME: Temporary measure
    double hv[3]; // Depths at vertices
    double uc, vc; // Centroid speeds

    // Compute linear combination between w-limited stages and
    // h-limited stages close to the bed elevation.

    #pragma hmppcg gridify(k), &
    #pragma hmppcg & private( k3, i, dz, hmin, alpha, h_diff, hc_k, &
    #pragma hmppcg & epsilon, hv, uc, vc), &
    #pragma hmppcg & global( H0, alpha_balance, tight_slope_limiters, &
    #pragma hmppcg & use_centroid_velocities, wc, zc, wv, zv, &
    #pragma hmppcg & xmomc, ymomc, xmomv, ymomv)
    for (k = 0; k < N; k++) {
        // Compute maximal variation in bed elevation
        // This quantitiy is
        //     dz = max_i abs(z_i - z_c)
        // and it is independent of dimension
        // In the 1d case zc = (z0+z1)/2
        // In the 2d case zc = (z0+z1+z2)/3

        k3 = 3 * k;
        hc_k = wc[k] - zc[k]; // Centroid value at triangle k

        dz = 0.0;
        if (tight_slope_limiters == 0) {
            // FIXME: Try with this one precomputed
            for (i = 0; i < 3; i++) {
                dz = fmax(dz, fabs(zv[k3 + i] - zc[k]));
            }
        }

        // Calculate depth at vertices (possibly negative here!)
        hv[0] = wv[k3] - zv[k3];
        hv[1] = wv[k3 + 1] - zv[k3 + 1];
        hv[2] = wv[k3 + 2] - zv[k3 + 2];

        // Calculate minimal depth across all three vertices
        hmin = fmin(hv[0], fmin(hv[1], hv[2]));

        //if (hmin < 0.0 ) {
        //  printf("hmin = %f\n",hmin);
        //}


        // Create alpha in [0,1], where alpha==0 means using the h-limited
        // stage and alpha==1 means using the w-limited stage as
        // computed by the gradient limiter (both 1st or 2nd order)
        if (tight_slope_limiters == 0) {
            // If hmin > dz/alpha_balance then alpha = 1 and the bed will have no
            // effect
            // If hmin < 0 then alpha = 0 reverting to constant height above bed.
            // The parameter alpha_balance==2 by default


            if (dz > 0.0) {
                alpha = fmax(fmin(alpha_balance * hmin / dz, 1.0), 0.0);
            } else {
                alpha = 1.0; // Flat bed
            }
            //printf("Using old style limiter\n");

        } else {

            // Tight Slope Limiter (2007)

            // Make alpha as large as possible but still ensure that
            // final depth is positive and that velocities at vertices
            // are controlled

            if (hmin < H0) {
                alpha = 1.0;
                for (i = 0; i < 3; i++) {

                    h_diff = hc_k - hv[i];
                    if (h_diff <= 0) {
                        // Deep water triangle is further away from bed than
                        // shallow water (hbar < h). Any alpha will do

                    } else {
                        // Denominator is positive which means that we need some of the
                        // h-limited stage.

                        alpha = fmin(alpha, (hc_k - H0) / h_diff);
                    }
                }

                // Ensure alpha in [0,1]
                if (alpha > 1.0) alpha = 1.0;
                if (alpha < 0.0) alpha = 0.0;

            } else {
                // Use w-limited stage exclusively in deeper water.
                alpha = 1.0;
            }
        }


        //  Let
        //
        //    wvi be the w-limited stage (wvi = zvi + hvi)
        //    wvi- be the h-limited state (wvi- = zvi + hvi-)
        //
        //
        //  where i=0,1,2 denotes the vertex ids
        //
        //  Weighted balance between w-limited and h-limited stage is
        //
        //    wvi := (1-alpha)*(zvi+hvi-) + alpha*(zvi+hvi)
        //
        //  It follows that the updated wvi is
        //    wvi := zvi + (1-alpha)*hvi- + alpha*hvi
        //
        //   Momentum is balanced between constant and limited


        if (alpha < 1) {
            for (i = 0; i < 3; i++) {

                wv[k3 + i] = zv[k3 + i] + (1 - alpha) * hc_k + alpha * hv[i];

                // Update momentum at vertices
                if (use_centroid_velocities == 1) {
                    // This is a simple, efficient and robust option
                    // It uses first order approximation of velocities, but retains
                    // the order used by stage.

                    // Speeds at centroids
                    if (hc_k > epsilon) {
                        uc = xmomc[k] / hc_k;
                        vc = ymomc[k] / hc_k;
                    } else {
                        uc = 0.0;
                        vc = 0.0;
                    }

                    // Vertex momenta guaranteed to be consistent with depth guaranteeing
                    // controlled speed
                    hv[i] = wv[k3 + i] - zv[k3 + i]; // Recompute (balanced) vertex depth
                    xmomv[k3 + i] = uc * hv[i];
                    ymomv[k3 + i] = vc * hv[i];

                } else {
                    // Update momentum as a linear combination of
                    // xmomc and ymomc (shallow) and momentum
                    // from extrapolator xmomv and ymomv (deep).
                    // This assumes that values from xmomv and ymomv have
                    // been established e.g. by the gradient limiter.

                    // FIXME (Ole): I think this should be used with vertex momenta
                    // computed above using centroid_velocities instead of xmomc
                    // and ymomc as they'll be more representative first order
                    // values.

                    xmomv[k3 + i] = (1 - alpha) * xmomc[k] + alpha * xmomv[k3 + i];
                    ymomv[k3 + i] = (1 - alpha) * ymomc[k] + alpha * ymomv[k3 + i];

                }
            }
        }
    }
}
