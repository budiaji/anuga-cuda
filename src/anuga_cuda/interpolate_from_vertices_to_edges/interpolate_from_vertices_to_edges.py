#!/usr/bin/env python
import numpy
from pycuda import driver as drv
from anuga_cuda import *


using_rearranged_domain = True

domain1 = generate_merimbula_domain( gpu=False )
domain2 = generate_merimbula_domain( gpu=True )

if using_rearranged_domain:
    domain2 = rearrange_domain(domain2)
    sort_domain(domain1)



domain2.equip_kernel_functions()

domain1.protect_against_infinitesimal_and_negative_heights()
domain2.protect_against_infinitesimal_and_negative_heights()

if domain1.optimised_gradient_limiter:
    if domain1._order_ == 1:
        for name in domain1.conserved_quantities:
            Q1 = domain1.quantities[name]
            Q1.extrapolate_first_order()
            
            Q2 = domain2.quantities[name]
            Q2.extrapolate_first_order()
    
    
    elif domain1._order_ == 2:
        domain1.extrapolate_second_order_sw()
        domain2.extrapolate_second_order_sw()
else:
    for name in domain1.conserved_quantities:
        Q1 = domain1.quantities[name]
        Q2 = domain1.quantities[name]
        if domain1._order_ == 1:
            Q1.extrapolate_first_order()
            Q2.extrapolate_first_order()
        elif domain1._order_ == 2:
            Q1.extrapolate_second_order_and_limit_by_vertex()
            Q2.extrapolate_second_order_and_limit_by_vertex()
        
N = domain1.quantities['stage'].vertex_values.shape[0]
print N
import sys
W1 = 0
for i in range( len(sys.argv)):
    if sys.argv[i] == "-b":
        W1 = int(sys.argv[i+1])

if not W1:
    W1 = domain2.interpolate_from_vertices_to_edges_func.max_threads_per_block
print W1
W2 = 1
W3 = 1

get_kernel_function_info(domain2.interpolate_from_vertices_to_edges_func,
    W1,W2, W3)


domain1.balance_deep_and_shallow()
domain2.balance_deep_and_shallow()

for name in domain1.conserved_quantities:
    Q1 = domain1.quantities[name]
    Q1.interpolate_from_vertices_to_edges()

    Q2 = domain2.quantities[name]
    domain2.interpolate_from_vertices_to_edges_func(
        numpy.int32( N ),
        drv.In( Q2.vertex_values ),
        drv.InOut( Q2.edge_values ),
        block = (W1, W2, W3),
        grid = ( (N + W1*W2*W3 -1) / (W1*W2*W3), 1)
        )

    #print name
    #print Q1.edge_values, Q2.edge_values
    if not numpy.allclose(Q1.edge_values, Q2.edge_values):
        for i in range(N):
            if not numpy.allclose(Q1.edge_values[i], Q2.edge_values[i]):
                print i, Q1.edge_values[i], Q2.edge_values[i]
    else:
        print True
