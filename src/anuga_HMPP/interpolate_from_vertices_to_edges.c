#include "hmpp_fun.h"



#ifdef USING_LOCAL_DIRECTIVES
#pragma hmpp interpolateVtoE codelet, target=CUDA args[*].transfer=atcall
#endif
void interpolate_from_vertices_to_edges(
        int N,
        int N3,
        double vertex_values[N3],
        double edge_values[N3]) 
{
    int k, k3;

    double q0, q1, q2;
    
    #pragma hmppcg gridify(k), private(q0, q1, q2, k3), &
    #pragma hmppcg & global(vertex_values, edge_values)
    for (k=0; k<N; k++) {

#ifndef REARRANGED_DOMAIN
        k3 = k*3;
        q0 = vertex_values[k3 + 0];
        q1 = vertex_values[k3 + 1];
        q2 = vertex_values[k3 + 2];

        edge_values[k3 + 0] = 0.5*(q1+q2);
        edge_values[k3 + 1] = 0.5*(q0+q2);
        edge_values[k3 + 2] = 0.5*(q0+q1);
#else
        q0 = vertex_values[k];
        q1 = vertex_values[k + N];
        q2 = vertex_values[k + 2*N];

        edge_values[k] = 0.5*(q1+q2);
        edge_values[k + N] = 0.5*(q0+q2);
        edge_values[k + 2*N] = 0.5*(q0+q1);
#endif
    }
}
