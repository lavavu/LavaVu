#ifndef Types__
#define Types__

/* Data types for geometry blocks */
typedef enum
{
   lucMinType,
   lucLabelType = lucMinType,
   lucPointType,
   lucGridType,
   lucTriangleType,
   lucVectorType,
   lucTracerType,
   lucLineType,
   lucShapeType,
   lucVolumeType,
   lucTubeType,
   lucMaxType
} lucGeometryType;

/* Data types for values to map to visual properties of geometry */
typedef enum
{
   lucMinDataType,
   lucVertexData = lucMinDataType,
   lucNormalData,
   lucVectorData,
   lucColourValueData,
   lucOpacityValueData,
   lucRedValueData,
   lucGreenValueData,
   lucBlueValueData,
   lucIndexData,
   lucXWidthData,
   lucYHeightData,
   lucZLengthData,
   lucRGBAData,
   lucTexCoordData,
   lucSizeData,
   lucPositionData,
   lucIdData,
   lucMaxDataType
} lucGeometryDataType;

#endif /* Types__ */
