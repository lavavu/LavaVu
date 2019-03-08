## Command line arguments

|         | General options
| ------- | ---------------
| -#      | Any integer entered as a switch will be interpreted as the initial timestep to load. Any following integer switch will be interpreted as the final timestep for output. eg: -10 -20 will run all output commands on timestep 10 to 20 inclusive
| -c#     | caching, set # of timesteps to cache data in memory for
| -A      | All objects hidden initially, use 'show object' to display
| -N      | No load, deferred loading mode, use 'load object' to load & display from database
| -S      | Skip default script, don't run init.script
| -v      | Verbose output, debugging info displayed to console
| -a      | Automation mode, don't activate event processing loop
| -p#     | port, web server interface listen on port #
| -q#     | quality, web server jpeg quality (0=don't serve images)
| -n#     | number of threads to launch for web server #
| -Q      | quiet mode, no status updates to screen
| -s      | stereo, request a quad-buffer stereo context

|         | Model options
| ------- | -------------
| -f#     | Load initial figure number # [1-n]
| -C      | Global camera, each model shares the same camera view
| -y      | Swap z/y axes of imported static models
| -T#     | Subdivide imported static model triangles into #
| -V#,#,# | Volume data resolution in X,Y,Z
| -e      | Each data set loaded from non time varying source has new timestep inserted

|         | Image/Video output
| ------- | ------------------
| -z#     | Render images # times larger and downsample for output
| -i/w    | Write images of all loaded timesteps/windows then exit
| -I/W    | Write images as above but using input database path as output path for images
| -u      | Returns images encoded as string data (for library use)
| -U      | Returns json model data encoded string (for library use)
| -t      | Write transparent background png images (if supported)
| -m#     | Write movies of all loaded timesteps/windows #=fps(30) (if supported)
| -x#,#   | Set output image width, height (height will be calculated if omitted)

|         | Data Export
| ------- | -----------
| -d#     | Export object id # to CSV vertices + values
| -j#     | Export object id # to JSON, if # omitted will output all compatible objects
| -g#     | Export object id # to GLDB (zlib compressed), if # omitted will output all compatible objects
| -G#     | Export object id # to GLDB (uncompressed), if # omitted will output all compatible objects

|         | Window Settings
| ------- | ---------------
| -r#,#   | Resize initial viewer window width, height
| -h      | hidden window, will exit after running any provided input script and output options
