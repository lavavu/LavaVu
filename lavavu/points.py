"""
Warning! EXPERIMENTAL:
 these features and functions are under development, will have bugs,
 and may be heavily modified in the future

Tools for importing and working with point clouds
- OBJ loader, requires pywavefront
- PLY loader, requires plyfile
- LAS loader, requires laspy

These functions ported from my viz script repo here:
https://github.com/mivp/vizscripts/tree/master/pointcloud
More tools can be found there, including a ptx reader which handles transformations of multiple point clouds into one coord space

"""
import numpy
import os
import convert

def loadpointcloud(filename, subsample=1):
    """
    Attempt to load the passed point cloud file based
    Pick loader based on extension

    Returns
    -------
    vertices : array
        numpy array x,y,z
    colours : array
        numpy array r,g,b or r,g,b,a
    """
    fn, ext = os.path.splitext(filename)
    ext = ext.lower()
    if ext == '.obj':
        print("Loading OBJ")
        import pywavefront
        scene = pywavefront.Wavefront(filename)
        V = numpy.array(scene.vertices)

        #SubSample
        if subsample > 1:
            V = V[::subsample]
            print("Subsampled:",V.shape)

        verts = V[:,0:3]
        colours = V[:,3:] * 255

        #Load positions and RGB
        print("Creating visualisation ")
        return (verts, colours)

    elif ext == '.ply':
        print("Loading PLY")
        from plyfile import PlyData, PlyElement       
        plydata = PlyData.read(filename)
        if plydata:
            x = plydata['vertex']['x']
            y = plydata['vertex']['y']
            z = plydata['vertex']['z']
            V = numpy.vstack((x,y,z)).reshape([3, -1]).transpose()

            C = convert._get_PLY_colours(plydata.elements[0])

            #SubSample
            if subsample > 1:
                V = V[::subsample]
                if C is not None:
                    C = C[::subsample]

            return (V, C)

    elif ext == '.las':
        print("Loading LAS")
        import laspy
        infile = laspy.file.File(filename, mode="r")
        '''
        print(infile)
        print(infile.point_format)
        for spec in infile.point_format:
           print(spec.name)
        print(infile.header)
        print(infile.header.dataformat_id)

        print(infile.header.offset)
        print(infile.header.scale)
        '''

        #Grab all of the points from the file.
        #point_records = infile.points
        #print(point_records)
        #print(point_records.shape)

        #Convert colours from short to uchar
        if infile.red.dtype == numpy.uint16:
            R = (infile.red / 255).astype(numpy.uint8)
            G = (infile.green / 255).astype(numpy.uint8)
            B = (infile.blue / 255).astype(numpy.uint8)
        else:
            R = infile.red
            G = infile.green
            B = infile.blue

        ss = subsample
        if ss > 1:
            V = numpy.array([infile.x[::ss], infile.y[::ss], infile.z[::ss]])
            C = numpy.array([R[::ss],G[::ss],B[::ss]])
            return (V, C)
        else:
            V = numpy.array([infile.x, infile.y, infile.z])
            C = numpy.array([R,G,B])
            return (V, C)

    else:
        print("Unknown point cloud format, extension: ", ext)

