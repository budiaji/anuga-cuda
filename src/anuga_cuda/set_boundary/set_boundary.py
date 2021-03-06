#!/usr/bin/env python


import numpy
from pycuda import driver as drv
from anuga_cuda import *


using_tsunami_domain = False
using_rearranged_domain =True

if using_tsunami_domain:
    domain1 = generate_cairns_domain(False)
    domain2 = generate_cairns_domain(True)
else:
    domain1 = generate_merimbula_domain(False)
    domain2 = generate_merimbula_domain(True)

    #domain1 = generate_channel3_domain()
    #domain2 = generate_channel3_domain(gpu=True)
if using_rearranged_domain:
    domain2 = rearrange_domain(domain2)
    sort_domain(domain1)

domain2.equip_kernel_functions()

N = domain2.boundary_cells.shape[0]
print " Number of elements: %d" % N
import sys
W1 = 0
for i in range( len(sys.argv)):
    if sys.argv[i] == "-b":
        W1 = int(sys.argv[i+1])

if not W1:
    W1 = domain2.set_boundary_values_from_edges_func.max_threads_per_block
W2 = 1
W3 = 1

get_kernel_function_info(domain2.set_boundary_values_from_edges_func, 
        W1,W2, W3)



Z1 = domain1.quantities['elevation']
Z2 = domain2.quantities['elevation']

print "quantity_boundary_values is all close? ", \
    numpy.allclose(Z1.boundary_values, Z2.boundary_values)
#print Z1.boundary_values
print "quantity_edge_values is all close? ", \
    numpy.allclose(Z1.edge_values, Z2.edge_values)
#print Z1.edge_values
print "boundary_cells is all close? ", \
    numpy.allclose(domain1.boundary_cells, domain2.boundary_cells)
print "boundary_edges is all close? ", \
    numpy.allclose(domain1.boundary_edges, domain2.boundary_edges)

Z1.set_boundary_values_from_edges()
domain2.set_boundary_values_from_edges_func(
        numpy.int32(N),
        drv.In( domain2.boundary_cells ),
        drv.In( domain2.boundary_edges ),
        drv.InOut( domain2.quantities['elevation'].boundary_values ),
        drv.In( domain2.quantities['elevation'].edge_values ),
        block = (W1, W2, W3),
        grid=((N+W1*W2*W3-1)/(W1*W2*W3),1)
        )

#print Z1.boundary_values, Z2.boundary_values
res =  numpy.allclose(Z1.boundary_values, Z2.boundary_values)

print "boundary_values all closed? " ,res

if not res:
    cnt_z = 0
    res = []
    z1 = Z1.boundary_values
    z2 = Z2.boundary_values
    for i in range(N):
        if z1[i] != z2[i]:
            cnt_z += 1
            res.append(z2[i])
            if cnt_z <10:
                print i, z1[i], z2[i]
    print " Number of diff: is %d " % cnt_z
    #print res
