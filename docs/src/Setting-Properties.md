## Setting properties

(This applies to the interactive viewer, from python use the Viewer or Object classes like a dictionary to set properties instead, or pass initial properties to the constructors)

Most of the variables controlling display of objects and views are controlled by properties (see: Properties reference)
None of the properties are case sensitive and will be converted to lower case when processing.

These properties can be changed interactively or in a script by entering the property name followed by = then the desired value

If an object is selected the property is applied to that object

    select myobj
    opacity=0.5

If no object is selected, either the property is applicable to the current view/camera and it is applied there, or otherwise a global property is set.

    border=0    #Applies to view, disabling border
    opacity=0.7 #Applies globally, default opacity

Properties that apply to objects can also be set globally, if the object doesn't have a value set, it will use the global value if found, falling back to a default value if not.

Numerical properties can be incremented or decremented as follows:

    pointsize+=1
    opacity-=0.1

Boolean properties can be set to 1/0 or true/false

Colour properties can be specified by html style strings, [X11 colour names](https://en.wikipedia.org/wiki/X11_color_names#Color_name_chart) or json RGBA arrays

    colour=#ff0000
    colour=red
    colour=rgba(255,0,0,1)
    colour=[1,0,0,1]

From Underworld 2, properties are passed to LavaVu as additional arguments when creating figures or objects.
To set a property, pass to the object (for individual object properties) or figure (for global/view properties):

    fig = glucifer.Figure(margin=50)
    arrows = glucifer.objects.VectorArrows(mesh, velocityField, arrowhead=2)

To update the properties of an existing object, simply modify as you would a python dict:

    arrows["arrowhead"] = 3
    fig["title"] = "New Title"
