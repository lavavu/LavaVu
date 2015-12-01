#include "GraphicsUtil.h"
#include "Colours.h"

void Colour_SetColour(Colour* colour)
{
   glColor4ubv(colour->rgba);
}

void Colour_Invert(Colour& colour)
{
   int alpha = colour.value & 0xff000000;
   colour.value = (~colour.value & 0x00ffffff) | alpha;
}

Colour parseRGBA(std::string value)
{
   //Parse rgba(r,g,b,a) values
   Colour col;
   int c;
   float alpha;
   try
   {
      std::stringstream ss(value.substr(5));
      for (int i=0; i<3; i++)
      {
         ss >> c;
         col.rgba[i] = c;
         char next = ss.peek();
         if (next == ',' || next == ' ')
            ss.ignore();
      }
      ss >> alpha;
      if (alpha > 1.)
         col.a = alpha;
      else
        col.a = 255 * alpha;
   }
   catch (std::exception& e)
   {
      std::cerr << "Failed to parse rgba colour: " << value << " : " << e.what() << std::endl;
   }
   return col; //rgba(c[0],c[1],c[2],c[3]);
}

Colour Colour_FromJson(json::Object& object, std::string key, GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
   Colour colour = {red, green, blue, alpha};
   if (!object.HasKey(key)) return colour;
   return Colour_FromJson(object[key], red, green, blue, alpha);
}

Colour Colour_FromJson(json::Value& value, GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
   Colour colour = {red, green, blue, alpha};
   //Will accept integer colour or [r,g,b,a] array or string containing x11 name or #hex)
   if (value.GetType() == json::IntVal)
   {
      colour.value = value.ToInt();
   }
   else if (value.GetType() == json::ArrayVal)
   {
      json::Array array = value.ToArray();
      if (array[0].ToFloat(0) > 1.0)
         colour.r = array[0].ToInt(0);
      else
         colour.r = array[0].ToFloat(0)*255.0;
      if (array[1].ToFloat(0) > 1.0)
         colour.g = array[1].ToInt(0);
      else
         colour.g = array[1].ToFloat(0)*255.0;
      if (array[2].ToFloat(0) > 1.0)
         colour.b = array[2].ToInt(0);
      else
         colour.b = array[2].ToFloat(0)*255.0;

      if (array.size() > 3)
         colour.a = array[3].ToFloat(0)*255.0;
   }
   else if (value.GetType() == json::StringVal)
   {
      return Colour_FromString(value.ToString());
   }

   return colour;
}

json::Value Colour_ToJson(int colourval)
{
   Colour colour;
   colour.value = colourval;
   return Colour_ToJson(colour);
}

json::Value Colour_ToJson(Colour& colour)
{
   json::Array array;
   array.push_back(colour.r/255.0); 
   array.push_back(colour.g/255.0); 
   array.push_back(colour.b/255.0); 
   array.push_back(colour.a/255.0); 
   return array;
}

void Colour_SetUniform(GLint uniform, Colour colour)
{
   float array[4];
   Colour_ToArray(colour, array);
   glUniform4fv(uniform, 1, array);
}

void Colour_ToArray(Colour colour, float* array)
{
   array[0] = colour.r/255.0;
   array[1] = colour.g/255.0;
   array[2] = colour.b/255.0;
   array[3] = colour.a/255.0;
}

Colour Colour_FromString(std::string str)
{
   char* charPointer;
   float opacity = 1.0;

   Colour colour = Colour_FromX11Colour(str);

   /* Get Opacity From String */
   /* Opacity must be read in after the ":" of the name of the colour */
   int pos = str.find(":");
   if (pos != std::string::npos)
   {
      std::stringstream ss(str.substr(pos+1));
      if (!(ss >> opacity))
         opacity = 1.0;
   }

   colour.a = opacity * 255;
   return colour;
}

/* Reads hex or colour from X11 Colour Chart */
/* Defaults to black if anything else */

Colour Colour_FromX11Colour(std::string x11Colour)
{
   Colour colour;
   const char* x11ColourName = x11Colour.c_str();

   if (strncmp(x11ColourName,"#",1) == 0)
   {
      return Colour_FromHex(x11ColourName);
   }
   else if (strncasecmp(x11ColourName,"snow",4) == 0)
   {
      colour.r = 255;
      colour.g = 250;
      colour.b = 250;
   }
   else if (strncasecmp(x11ColourName,"GhostWhite",10) == 0)
   {
      colour.r = 248;
      colour.g = 248;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"WhiteSmoke",10) == 0)
   {
      colour.r = 245;
      colour.g = 245;
      colour.b = 245;
   }
   else if (strncasecmp(x11ColourName,"gainsboro",9) == 0)
   {
      colour.r = 220;
      colour.g = 220;
      colour.b = 220;
   }
   else if (strncasecmp(x11ColourName,"FloralWhite",11) == 0)
   {
      colour.r = 255;
      colour.g = 250;
      colour.b = 240;
   }
   else if (strncasecmp(x11ColourName,"OldLace",7) == 0)
   {
      colour.r = 253;
      colour.g = 245;
      colour.b = 230;
   }
   else if (strncasecmp(x11ColourName,"linen",5) == 0)
   {
      colour.r = 250;
      colour.g = 240;
      colour.b = 230;
   }
   else if (strncasecmp(x11ColourName,"AntiqueWhite",12) == 0)
   {
      colour.r = 250;
      colour.g = 235;
      colour.b = 215;
   }
   else if (strncasecmp(x11ColourName,"PapayaWhip",10) == 0)
   {
      colour.r = 255;
      colour.g = 239;
      colour.b = 213;
   }
   else if (strncasecmp(x11ColourName,"BlanchedAlmond",14) == 0)
   {
      colour.r = 255;
      colour.g = 235;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"bisque",6) == 0)
   {
      colour.r = 255;
      colour.g = 228;
      colour.b = 196;
   }
   else if (strncasecmp(x11ColourName,"PeachPuff",9) == 0)
   {
      colour.r = 255;
      colour.g = 218;
      colour.b = 185;
   }
   else if (strncasecmp(x11ColourName,"NavajoWhite",11) == 0)
   {
      colour.r = 255;
      colour.g = 222;
      colour.b = 173;
   }
   else if (strncasecmp(x11ColourName,"moccasin",8) == 0)
   {
      colour.r = 255;
      colour.g = 228;
      colour.b = 181;
   }
   else if (strncasecmp(x11ColourName,"cornsilk",8) == 0)
   {
      colour.r = 255;
      colour.g = 248;
      colour.b = 220;
   }
   else if (strncasecmp(x11ColourName,"ivory",5) == 0)
   {
      colour.r = 255;
      colour.g = 255;
      colour.b = 240;
   }
   else if (strncasecmp(x11ColourName,"LemonChiffon",12) == 0)
   {
      colour.r = 255;
      colour.g = 250;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"seashell",8) == 0)
   {
      colour.r = 255;
      colour.g = 245;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"honeydew",8) == 0)
   {
      colour.r = 240;
      colour.g = 255;
      colour.b = 240;
   }
   else if (strncasecmp(x11ColourName,"MintCream",9) == 0)
   {
      colour.r = 245;
      colour.g = 255;
      colour.b = 250;
   }
   else if (strncasecmp(x11ColourName,"azure",5) == 0)
   {
      colour.r = 240;
      colour.g = 255;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"AliceBlue",9) == 0)
   {
      colour.r = 240;
      colour.g = 248;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"lavender",8) == 0)
   {
      colour.r = 230;
      colour.g = 230;
      colour.b = 250;
   }
   else if (strncasecmp(x11ColourName,"LavenderBlush",13) == 0)
   {
      colour.r = 255;
      colour.g = 240;
      colour.b = 245;
   }
   else if (strncasecmp(x11ColourName,"MistyRose",9) == 0)
   {
      colour.r = 255;
      colour.g = 228;
      colour.b = 225;
   }
   else if (strncasecmp(x11ColourName,"white",5) == 0)
   {
      colour.r = 255;
      colour.g = 255;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"black",5) == 0)
   {
      colour.r = 0;
      colour.g = 0;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"DarkSlateGray",13) == 0)
   {
      colour.r = 47;
      colour.g = 79;
      colour.b = 79;
   }
   else if (strncasecmp(x11ColourName,"DarkSlateGrey",13) == 0)
   {
      colour.r = 47;
      colour.g = 79;
      colour.b = 79;
   }
   else if (strncasecmp(x11ColourName,"DimGray",7) == 0)
   {
      colour.r = 105;
      colour.g = 105;
      colour.b = 105;
   }
   else if (strncasecmp(x11ColourName,"DimGrey",7) == 0)
   {
      colour.r = 105;
      colour.g = 105;
      colour.b = 105;
   }
   else if (strncasecmp(x11ColourName,"SlateGray",9) == 0)
   {
      colour.r = 112;
      colour.g = 128;
      colour.b = 144;
   }
   else if (strncasecmp(x11ColourName,"SlateGrey",9) == 0)
   {
      colour.r = 112;
      colour.g = 128;
      colour.b = 144;
   }
   else if (strncasecmp(x11ColourName,"LightSlateGray",14) == 0)
   {
      colour.r = 119;
      colour.g = 136;
      colour.b = 153;
   }
   else if (strncasecmp(x11ColourName,"LightSlateGrey",14) == 0)
   {
      colour.r = 119;
      colour.g = 136;
      colour.b = 153;
   }
   else if (strncasecmp(x11ColourName,"gray",4) == 0)
   {
      colour.r = 190;
      colour.g = 190;
      colour.b = 190;
   }
   else if (strncasecmp(x11ColourName,"grey",4) == 0)
   {
      colour.r = 190;
      colour.g = 190;
      colour.b = 190;
   }
   else if (strncasecmp(x11ColourName,"LightGrey",9) == 0)
   {
      colour.r = 211;
      colour.g = 211;
      colour.b = 211;
   }
   else if (strncasecmp(x11ColourName,"LightGray",9) == 0)
   {
      colour.r = 211;
      colour.g = 211;
      colour.b = 211;
   }
   else if (strncasecmp(x11ColourName,"MidnightBlue",12) == 0)
   {
      colour.r = 25;
      colour.g = 25;
      colour.b = 112;
   }
   else if (strncasecmp(x11ColourName,"navy",4) == 0)
   {
      colour.r = 0;
      colour.g = 0;
      colour.b = 128;
   }
   else if (strncasecmp(x11ColourName,"NavyBlue",8) == 0)
   {
      colour.r = 0;
      colour.g = 0;
      colour.b = 128;
   }
   else if (strncasecmp(x11ColourName,"CornflowerBlue",14) == 0)
   {
      colour.r = 100;
      colour.g = 149;
      colour.b = 237;
   }
   else if (strncasecmp(x11ColourName,"DarkSlateBlue",13) == 0)
   {
      colour.r = 72;
      colour.g = 61;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"SlateBlue",9) == 0)
   {
      colour.r = 106;
      colour.g = 90;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"MediumSlateBlue",15) == 0)
   {
      colour.r = 123;
      colour.g = 104;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"LightSlateBlue",14) == 0)
   {
      colour.r = 132;
      colour.g = 112;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"MediumBlue",10) == 0)
   {
      colour.r = 0;
      colour.g = 0;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"RoyalBlue",9) == 0)
   {
      colour.r = 65;
      colour.g = 105;
      colour.b = 225;
   }
   else if (strncasecmp(x11ColourName,"blue",4) == 0)
   {
      colour.r = 0;
      colour.g = 0;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"DodgerBlue",10) == 0)
   {
      colour.r = 30;
      colour.g = 144;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"DeepSkyBlue",11) == 0)
   {
      colour.r = 0;
      colour.g = 191;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"SkyBlue",7) == 0)
   {
      colour.r = 135;
      colour.g = 206;
      colour.b = 235;
   }
   else if (strncasecmp(x11ColourName,"LightSkyBlue",12) == 0)
   {
      colour.r = 135;
      colour.g = 206;
      colour.b = 250;
   }
   else if (strncasecmp(x11ColourName,"SteelBlue",9) == 0)
   {
      colour.r = 70;
      colour.g = 130;
      colour.b = 180;
   }
   else if (strncasecmp(x11ColourName,"LightSteelBlue",14) == 0)
   {
      colour.r = 176;
      colour.g = 196;
      colour.b = 222;
   }
   else if (strncasecmp(x11ColourName,"LightBlue",9) == 0)
   {
      colour.r = 173;
      colour.g = 216;
      colour.b = 230;
   }
   else if (strncasecmp(x11ColourName,"PowderBlue",10) == 0)
   {
      colour.r = 176;
      colour.g = 224;
      colour.b = 230;
   }
   else if (strncasecmp(x11ColourName,"PaleTurquoise",13) == 0)
   {
      colour.r = 175;
      colour.g = 238;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"DarkTurquoise",13) == 0)
   {
      colour.r = 0;
      colour.g = 206;
      colour.b = 209;
   }
   else if (strncasecmp(x11ColourName,"MediumTurquoise",15) == 0)
   {
      colour.r = 72;
      colour.g = 209;
      colour.b = 204;
   }
   else if (strncasecmp(x11ColourName,"turquoise",9) == 0)
   {
      colour.r = 64;
      colour.g = 224;
      colour.b = 208;
   }
   else if (strncasecmp(x11ColourName,"cyan",4) == 0)
   {
      colour.r = 0;
      colour.g = 255;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"LightCyan",9) == 0)
   {
      colour.r = 224;
      colour.g = 255;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"CadetBlue",9) == 0)
   {
      colour.r = 95;
      colour.g = 158;
      colour.b = 160;
   }
   else if (strncasecmp(x11ColourName,"MediumAquamarine",16) == 0)
   {
      colour.r = 102;
      colour.g = 205;
      colour.b = 170;
   }
   else if (strncasecmp(x11ColourName,"aquamarine",10) == 0)
   {
      colour.r = 127;
      colour.g = 255;
      colour.b = 212;
   }
   else if (strncasecmp(x11ColourName,"DarkGreen",9) == 0)
   {
      colour.r = 0;
      colour.g = 100;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"DarkOliveGreen",14) == 0)
   {
      colour.r = 85;
      colour.g = 107;
      colour.b = 47;
   }
   else if (strncasecmp(x11ColourName,"DarkSeaGreen",12) == 0)
   {
      colour.r = 143;
      colour.g = 188;
      colour.b = 143;
   }
   else if (strncasecmp(x11ColourName,"SeaGreen",8) == 0)
   {
      colour.r = 46;
      colour.g = 139;
      colour.b = 87;
   }
   else if (strncasecmp(x11ColourName,"MediumSeaGreen",14) == 0)
   {
      colour.r = 60;
      colour.g = 179;
      colour.b = 113;
   }
   else if (strncasecmp(x11ColourName,"LightSeaGreen",13) == 0)
   {
      colour.r = 32;
      colour.g = 178;
      colour.b = 170;
   }
   else if (strncasecmp(x11ColourName,"PaleGreen",9) == 0)
   {
      colour.r = 152;
      colour.g = 251;
      colour.b = 152;
   }
   else if (strncasecmp(x11ColourName,"SpringGreen",11) == 0)
   {
      colour.r = 0;
      colour.g = 255;
      colour.b = 127;
   }
   else if (strncasecmp(x11ColourName,"LawnGreen",9) == 0)
   {
      colour.r = 124;
      colour.g = 252;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"green",5) == 0)
   {
      colour.r = 0;
      colour.g = 255;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"chartreuse",10) == 0)
   {
      colour.r = 127;
      colour.g = 255;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"MediumSpringGreen",17) == 0)
   {
      colour.r = 0;
      colour.g = 250;
      colour.b = 154;
   }
   else if (strncasecmp(x11ColourName,"GreenYellow",11) == 0)
   {
      colour.r = 173;
      colour.g = 255;
      colour.b = 47;
   }
   else if (strncasecmp(x11ColourName,"LimeGreen",9) == 0)
   {
      colour.r = 50;
      colour.g = 205;
      colour.b = 50;
   }
   else if (strncasecmp(x11ColourName,"YellowGreen",11) == 0)
   {
      colour.r = 154;
      colour.g = 205;
      colour.b = 50;
   }
   else if (strncasecmp(x11ColourName,"ForestGreen",11) == 0)
   {
      colour.r = 34;
      colour.g = 139;
      colour.b = 34;
   }
   else if (strncasecmp(x11ColourName,"OliveDrab",9) == 0)
   {
      colour.r = 107;
      colour.g = 142;
      colour.b = 35;
   }
   else if (strncasecmp(x11ColourName,"DarkKhaki",9) == 0)
   {
      colour.r = 189;
      colour.g = 183;
      colour.b = 107;
   }
   else if (strncasecmp(x11ColourName,"khaki",5) == 0)
   {
      colour.r = 240;
      colour.g = 230;
      colour.b = 140;
   }
   else if (strncasecmp(x11ColourName,"PaleGoldenrod",13) == 0)
   {
      colour.r = 238;
      colour.g = 232;
      colour.b = 170;
   }
   else if (strncasecmp(x11ColourName,"LightGoldenrodYellow",20) == 0)
   {
      colour.r = 250;
      colour.g = 250;
      colour.b = 210;
   }
   else if (strncasecmp(x11ColourName,"LightYellow",11) == 0)
   {
      colour.r = 255;
      colour.g = 255;
      colour.b = 224;
   }
   else if (strncasecmp(x11ColourName,"yellow",6) == 0)
   {
      colour.r = 255;
      colour.g = 255;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"gold",4) == 0)
   {
      colour.r = 255;
      colour.g = 215;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"LightGoldenrod",14) == 0)
   {
      colour.r = 238;
      colour.g = 221;
      colour.b = 130;
   }
   else if (strncasecmp(x11ColourName,"goldenrod",9) == 0)
   {
      colour.r = 218;
      colour.g = 165;
      colour.b = 32;
   }
   else if (strncasecmp(x11ColourName,"DarkGoldenrod",13) == 0)
   {
      colour.r = 184;
      colour.g = 134;
      colour.b = 11;
   }
   else if (strncasecmp(x11ColourName,"RosyBrown",9) == 0)
   {
      colour.r = 188;
      colour.g = 143;
      colour.b = 143;
   }
   else if (strncasecmp(x11ColourName,"IndianRed",9) == 0)
   {
      colour.r = 205;
      colour.g = 92;
      colour.b = 92;
   }
   else if (strncasecmp(x11ColourName,"SaddleBrown",11) == 0)
   {
      colour.r = 139;
      colour.g = 69;
      colour.b = 19;
   }
   else if (strncasecmp(x11ColourName,"sienna",6) == 0)
   {
      colour.r = 160;
      colour.g = 82;
      colour.b = 45;
   }
   else if (strncasecmp(x11ColourName,"peru",4) == 0)
   {
      colour.r = 205;
      colour.g = 133;
      colour.b = 63;
   }
   else if (strncasecmp(x11ColourName,"burlywood",9) == 0)
   {
      colour.r = 222;
      colour.g = 184;
      colour.b = 135;
   }
   else if (strncasecmp(x11ColourName,"beige",5) == 0)
   {
      colour.r = 245;
      colour.g = 245;
      colour.b = 220;
   }
   else if (strncasecmp(x11ColourName,"wheat",5) == 0)
   {
      colour.r = 245;
      colour.g = 222;
      colour.b = 179;
   }
   else if (strncasecmp(x11ColourName,"SandyBrown",10) == 0)
   {
      colour.r = 244;
      colour.g = 164;
      colour.b = 96;
   }
   else if (strncasecmp(x11ColourName,"tan",3) == 0)
   {
      colour.r = 210;
      colour.g = 180;
      colour.b = 140;
   }
   else if (strncasecmp(x11ColourName,"chocolate",9) == 0)
   {
      colour.r = 210;
      colour.g = 105;
      colour.b = 30;
   }
   else if (strncasecmp(x11ColourName,"firebrick",9) == 0)
   {
      colour.r = 178;
      colour.g = 34;
      colour.b = 34;
   }
   else if (strncasecmp(x11ColourName,"brown",5) == 0)
   {
      colour.r = 165;
      colour.g = 42;
      colour.b = 42;
   }
   else if (strncasecmp(x11ColourName,"DarkSalmon",10) == 0)
   {
      colour.r = 233;
      colour.g = 150;
      colour.b = 122;
   }
   else if (strncasecmp(x11ColourName,"salmon",6) == 0)
   {
      colour.r = 250;
      colour.g = 128;
      colour.b = 114;
   }
   else if (strncasecmp(x11ColourName,"LightSalmon",11) == 0)
   {
      colour.r = 255;
      colour.g = 160;
      colour.b = 122;
   }
   else if (strncasecmp(x11ColourName,"orange",6) == 0)
   {
      colour.r = 255;
      colour.g = 165;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"DarkOrange",10) == 0)
   {
      colour.r = 255;
      colour.g = 140;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"coral",5) == 0)
   {
      colour.r = 255;
      colour.g = 127;
      colour.b = 80;
   }
   else if (strncasecmp(x11ColourName,"LightCoral",10) == 0)
   {
      colour.r = 240;
      colour.g = 128;
      colour.b = 128;
   }
   else if (strncasecmp(x11ColourName,"tomato",6) == 0)
   {
      colour.r = 255;
      colour.g = 99;
      colour.b = 71;
   }
   else if (strncasecmp(x11ColourName,"OrangeRed",9) == 0)
   {
      colour.r = 255;
      colour.g = 69;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"red",3) == 0)
   {
      colour.r = 255;
      colour.g = 0;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"HotPink",7) == 0)
   {
      colour.r = 255;
      colour.g = 105;
      colour.b = 180;
   }
   else if (strncasecmp(x11ColourName,"DeepPink",8) == 0)
   {
      colour.r = 255;
      colour.g = 20;
      colour.b = 147;
   }
   else if (strncasecmp(x11ColourName,"pink",4) == 0)
   {
      colour.r = 255;
      colour.g = 192;
      colour.b = 203;
   }
   else if (strncasecmp(x11ColourName,"LightPink",9) == 0)
   {
      colour.r = 255;
      colour.g = 182;
      colour.b = 193;
   }
   else if (strncasecmp(x11ColourName,"PaleVioletRed",13) == 0)
   {
      colour.r = 219;
      colour.g = 112;
      colour.b = 147;
   }
   else if (strncasecmp(x11ColourName,"maroon",6) == 0)
   {
      colour.r = 176;
      colour.g = 48;
      colour.b = 96;
   }
   else if (strncasecmp(x11ColourName,"MediumVioletRed",15) == 0)
   {
      colour.r = 199;
      colour.g = 21;
      colour.b = 133;
   }
   else if (strncasecmp(x11ColourName,"VioletRed",9) == 0)
   {
      colour.r = 208;
      colour.g = 32;
      colour.b = 144;
   }
   else if (strncasecmp(x11ColourName,"magenta",7) == 0)
   {
      colour.r = 255;
      colour.g = 0;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"violet",6) == 0)
   {
      colour.r = 238;
      colour.g = 130;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"plum",4) == 0)
   {
      colour.r = 221;
      colour.g = 160;
      colour.b = 221;
   }
   else if (strncasecmp(x11ColourName,"orchid",6) == 0)
   {
      colour.r = 218;
      colour.g = 112;
      colour.b = 214;
   }
   else if (strncasecmp(x11ColourName,"MediumOrchid",12) == 0)
   {
      colour.r = 186;
      colour.g = 85;
      colour.b = 211;
   }
   else if (strncasecmp(x11ColourName,"DarkOrchid",10) == 0)
   {
      colour.r = 153;
      colour.g = 50;
      colour.b = 204;
   }
   else if (strncasecmp(x11ColourName,"DarkViolet",10) == 0)
   {
      colour.r = 148;
      colour.g = 0;
      colour.b = 211;
   }
   else if (strncasecmp(x11ColourName,"BlueViolet",10) == 0)
   {
      colour.r = 138;
      colour.g = 43;
      colour.b = 226;
   }
   else if (strncasecmp(x11ColourName,"purple",6) == 0)
   {
      colour.r = 160;
      colour.g = 32;
      colour.b = 240;
   }
   else if (strncasecmp(x11ColourName,"MediumPurple",12) == 0)
   {
      colour.r = 147;
      colour.g = 112;
      colour.b = 219;
   }
   else if (strncasecmp(x11ColourName,"thistle",7) == 0)
   {
      colour.r = 216;
      colour.g = 191;
      colour.b = 216;
   }
   else if (strncasecmp(x11ColourName,"snow1",5) == 0)
   {
      colour.r = 255;
      colour.g = 250;
      colour.b = 250;
   }
   else if (strncasecmp(x11ColourName,"snow2",5) == 0)
   {
      colour.r = 238;
      colour.g = 233;
      colour.b = 233;
   }
   else if (strncasecmp(x11ColourName,"snow3",5) == 0)
   {
      colour.r = 205;
      colour.g = 201;
      colour.b = 201;
   }
   else if (strncasecmp(x11ColourName,"snow4",5) == 0)
   {
      colour.r = 139;
      colour.g = 137;
      colour.b = 137;
   }
   else if (strncasecmp(x11ColourName,"seashell1",9) == 0)
   {
      colour.r = 255;
      colour.g = 245;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"seashell2",9) == 0)
   {
      colour.r = 238;
      colour.g = 229;
      colour.b = 222;
   }
   else if (strncasecmp(x11ColourName,"seashell3",9) == 0)
   {
      colour.r = 205;
      colour.g = 197;
      colour.b = 191;
   }
   else if (strncasecmp(x11ColourName,"seashell4",9) == 0)
   {
      colour.r = 139;
      colour.g = 134;
      colour.b = 130;
   }
   else if (strncasecmp(x11ColourName,"AntiqueWhite1",13) == 0)
   {
      colour.r = 255;
      colour.g = 239;
      colour.b = 219;
   }
   else if (strncasecmp(x11ColourName,"AntiqueWhite2",13) == 0)
   {
      colour.r = 238;
      colour.g = 223;
      colour.b = 204;
   }
   else if (strncasecmp(x11ColourName,"AntiqueWhite3",13) == 0)
   {
      colour.r = 205;
      colour.g = 192;
      colour.b = 176;
   }
   else if (strncasecmp(x11ColourName,"AntiqueWhite4",13) == 0)
   {
      colour.r = 139;
      colour.g = 131;
      colour.b = 120;
   }
   else if (strncasecmp(x11ColourName,"bisque1",7) == 0)
   {
      colour.r = 255;
      colour.g = 228;
      colour.b = 196;
   }
   else if (strncasecmp(x11ColourName,"bisque2",7) == 0)
   {
      colour.r = 238;
      colour.g = 213;
      colour.b = 183;
   }
   else if (strncasecmp(x11ColourName,"bisque3",7) == 0)
   {
      colour.r = 205;
      colour.g = 183;
      colour.b = 158;
   }
   else if (strncasecmp(x11ColourName,"bisque4",7) == 0)
   {
      colour.r = 139;
      colour.g = 125;
      colour.b = 107;
   }
   else if (strncasecmp(x11ColourName,"PeachPuff1",10) == 0)
   {
      colour.r = 255;
      colour.g = 218;
      colour.b = 185;
   }
   else if (strncasecmp(x11ColourName,"PeachPuff2",10) == 0)
   {
      colour.r = 238;
      colour.g = 203;
      colour.b = 173;
   }
   else if (strncasecmp(x11ColourName,"PeachPuff3",10) == 0)
   {
      colour.r = 205;
      colour.g = 175;
      colour.b = 149;
   }
   else if (strncasecmp(x11ColourName,"PeachPuff4",10) == 0)
   {
      colour.r = 139;
      colour.g = 119;
      colour.b = 101;
   }
   else if (strncasecmp(x11ColourName,"NavajoWhite1",12) == 0)
   {
      colour.r = 255;
      colour.g = 222;
      colour.b = 173;
   }
   else if (strncasecmp(x11ColourName,"NavajoWhite2",12) == 0)
   {
      colour.r = 238;
      colour.g = 207;
      colour.b = 161;
   }
   else if (strncasecmp(x11ColourName,"NavajoWhite3",12) == 0)
   {
      colour.r = 205;
      colour.g = 179;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"NavajoWhite4",12) == 0)
   {
      colour.r = 139;
      colour.g = 121;
      colour.b = 94;
   }
   else if (strncasecmp(x11ColourName,"LemonChiffon1",13) == 0)
   {
      colour.r = 255;
      colour.g = 250;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"LemonChiffon2",13) == 0)
   {
      colour.r = 238;
      colour.g = 233;
      colour.b = 191;
   }
   else if (strncasecmp(x11ColourName,"LemonChiffon3",13) == 0)
   {
      colour.r = 205;
      colour.g = 201;
      colour.b = 165;
   }
   else if (strncasecmp(x11ColourName,"LemonChiffon4",13) == 0)
   {
      colour.r = 139;
      colour.g = 137;
      colour.b = 112;
   }
   else if (strncasecmp(x11ColourName,"cornsilk1",9) == 0)
   {
      colour.r = 255;
      colour.g = 248;
      colour.b = 220;
   }
   else if (strncasecmp(x11ColourName,"cornsilk2",9) == 0)
   {
      colour.r = 238;
      colour.g = 232;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"cornsilk3",9) == 0)
   {
      colour.r = 205;
      colour.g = 200;
      colour.b = 177;
   }
   else if (strncasecmp(x11ColourName,"cornsilk4",9) == 0)
   {
      colour.r = 139;
      colour.g = 136;
      colour.b = 120;
   }
   else if (strncasecmp(x11ColourName,"ivory1",6) == 0)
   {
      colour.r = 255;
      colour.g = 255;
      colour.b = 240;
   }
   else if (strncasecmp(x11ColourName,"ivory2",6) == 0)
   {
      colour.r = 238;
      colour.g = 238;
      colour.b = 224;
   }
   else if (strncasecmp(x11ColourName,"ivory3",6) == 0)
   {
      colour.r = 205;
      colour.g = 205;
      colour.b = 193;
   }
   else if (strncasecmp(x11ColourName,"ivory4",6) == 0)
   {
      colour.r = 139;
      colour.g = 139;
      colour.b = 131;
   }
   else if (strncasecmp(x11ColourName,"honeydew1",9) == 0)
   {
      colour.r = 240;
      colour.g = 255;
      colour.b = 240;
   }
   else if (strncasecmp(x11ColourName,"honeydew2",9) == 0)
   {
      colour.r = 224;
      colour.g = 238;
      colour.b = 224;
   }
   else if (strncasecmp(x11ColourName,"honeydew3",9) == 0)
   {
      colour.r = 193;
      colour.g = 205;
      colour.b = 193;
   }
   else if (strncasecmp(x11ColourName,"honeydew4",9) == 0)
   {
      colour.r = 131;
      colour.g = 139;
      colour.b = 131;
   }
   else if (strncasecmp(x11ColourName,"LavenderBlush1",14) == 0)
   {
      colour.r = 255;
      colour.g = 240;
      colour.b = 245;
   }
   else if (strncasecmp(x11ColourName,"LavenderBlush2",14) == 0)
   {
      colour.r = 238;
      colour.g = 224;
      colour.b = 229;
   }
   else if (strncasecmp(x11ColourName,"LavenderBlush3",14) == 0)
   {
      colour.r = 205;
      colour.g = 193;
      colour.b = 197;
   }
   else if (strncasecmp(x11ColourName,"LavenderBlush4",14) == 0)
   {
      colour.r = 139;
      colour.g = 131;
      colour.b = 134;
   }
   else if (strncasecmp(x11ColourName,"MistyRose1",10) == 0)
   {
      colour.r = 255;
      colour.g = 228;
      colour.b = 225;
   }
   else if (strncasecmp(x11ColourName,"MistyRose2",10) == 0)
   {
      colour.r = 238;
      colour.g = 213;
      colour.b = 210;
   }
   else if (strncasecmp(x11ColourName,"MistyRose3",10) == 0)
   {
      colour.r = 205;
      colour.g = 183;
      colour.b = 181;
   }
   else if (strncasecmp(x11ColourName,"MistyRose4",10) == 0)
   {
      colour.r = 139;
      colour.g = 125;
      colour.b = 123;
   }
   else if (strncasecmp(x11ColourName,"azure1",6) == 0)
   {
      colour.r = 240;
      colour.g = 255;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"azure2",6) == 0)
   {
      colour.r = 224;
      colour.g = 238;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"azure3",6) == 0)
   {
      colour.r = 193;
      colour.g = 205;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"azure4",6) == 0)
   {
      colour.r = 131;
      colour.g = 139;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"SlateBlue1",10) == 0)
   {
      colour.r = 131;
      colour.g = 111;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"SlateBlue2",10) == 0)
   {
      colour.r = 122;
      colour.g = 103;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"SlateBlue3",10) == 0)
   {
      colour.r = 105;
      colour.g = 89;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"SlateBlue4",10) == 0)
   {
      colour.r = 71;
      colour.g = 60;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"RoyalBlue1",10) == 0)
   {
      colour.r = 72;
      colour.g = 118;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"RoyalBlue2",10) == 0)
   {
      colour.r = 67;
      colour.g = 110;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"RoyalBlue3",10) == 0)
   {
      colour.r = 58;
      colour.g = 95;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"RoyalBlue4",10) == 0)
   {
      colour.r = 39;
      colour.g = 64;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"blue1",5) == 0)
   {
      colour.r = 0;
      colour.g = 0;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"blue2",5) == 0)
   {
      colour.r = 0;
      colour.g = 0;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"blue3",5) == 0)
   {
      colour.r = 0;
      colour.g = 0;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"blue4",5) == 0)
   {
      colour.r = 0;
      colour.g = 0;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"DodgerBlue1",11) == 0)
   {
      colour.r = 30;
      colour.g = 144;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"DodgerBlue2",11) == 0)
   {
      colour.r = 28;
      colour.g = 134;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"DodgerBlue3",11) == 0)
   {
      colour.r = 24;
      colour.g = 116;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"DodgerBlue4",11) == 0)
   {
      colour.r = 16;
      colour.g = 78;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"SteelBlue1",10) == 0)
   {
      colour.r = 99;
      colour.g = 184;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"SteelBlue2",10) == 0)
   {
      colour.r = 92;
      colour.g = 172;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"SteelBlue3",10) == 0)
   {
      colour.r = 79;
      colour.g = 148;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"SteelBlue4",10) == 0)
   {
      colour.r = 54;
      colour.g = 100;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"DeepSkyBlue1",12) == 0)
   {
      colour.r = 0;
      colour.g = 191;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"DeepSkyBlue2",12) == 0)
   {
      colour.r = 0;
      colour.g = 178;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"DeepSkyBlue3",12) == 0)
   {
      colour.r = 0;
      colour.g = 154;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"DeepSkyBlue4",12) == 0)
   {
      colour.r = 0;
      colour.g = 104;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"SkyBlue1",8) == 0)
   {
      colour.r = 135;
      colour.g = 206;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"SkyBlue2",8) == 0)
   {
      colour.r = 126;
      colour.g = 192;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"SkyBlue3",8) == 0)
   {
      colour.r = 108;
      colour.g = 166;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"SkyBlue4",8) == 0)
   {
      colour.r = 74;
      colour.g = 112;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"LightSkyBlue1",13) == 0)
   {
      colour.r = 176;
      colour.g = 226;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"LightSkyBlue2",13) == 0)
   {
      colour.r = 164;
      colour.g = 211;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"LightSkyBlue3",13) == 0)
   {
      colour.r = 141;
      colour.g = 182;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"LightSkyBlue4",13) == 0)
   {
      colour.r = 96;
      colour.g = 123;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"SlateGray1",10) == 0)
   {
      colour.r = 198;
      colour.g = 226;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"SlateGray2",10) == 0)
   {
      colour.r = 185;
      colour.g = 211;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"SlateGray3",10) == 0)
   {
      colour.r = 159;
      colour.g = 182;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"SlateGray4",10) == 0)
   {
      colour.r = 108;
      colour.g = 123;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"LightSteelBlue1",15) == 0)
   {
      colour.r = 202;
      colour.g = 225;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"LightSteelBlue2",15) == 0)
   {
      colour.r = 188;
      colour.g = 210;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"LightSteelBlue3",15) == 0)
   {
      colour.r = 162;
      colour.g = 181;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"LightSteelBlue4",15) == 0)
   {
      colour.r = 110;
      colour.g = 123;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"LightBlue1",10) == 0)
   {
      colour.r = 191;
      colour.g = 239;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"LightBlue2",10) == 0)
   {
      colour.r = 178;
      colour.g = 223;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"LightBlue3",10) == 0)
   {
      colour.r = 154;
      colour.g = 192;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"LightBlue4",10) == 0)
   {
      colour.r = 104;
      colour.g = 131;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"LightCyan1",10) == 0)
   {
      colour.r = 224;
      colour.g = 255;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"LightCyan2",10) == 0)
   {
      colour.r = 209;
      colour.g = 238;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"LightCyan3",10) == 0)
   {
      colour.r = 180;
      colour.g = 205;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"LightCyan4",10) == 0)
   {
      colour.r = 122;
      colour.g = 139;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"PaleTurquoise1",14) == 0)
   {
      colour.r = 187;
      colour.g = 255;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"PaleTurquoise2",14) == 0)
   {
      colour.r = 174;
      colour.g = 238;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"PaleTurquoise3",14) == 0)
   {
      colour.r = 150;
      colour.g = 205;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"PaleTurquoise4",14) == 0)
   {
      colour.r = 102;
      colour.g = 139;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"CadetBlue1",10) == 0)
   {
      colour.r = 152;
      colour.g = 245;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"CadetBlue2",10) == 0)
   {
      colour.r = 142;
      colour.g = 229;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"CadetBlue3",10) == 0)
   {
      colour.r = 122;
      colour.g = 197;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"CadetBlue4",10) == 0)
   {
      colour.r = 83;
      colour.g = 134;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"turquoise1",10) == 0)
   {
      colour.r = 0;
      colour.g = 245;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"turquoise2",10) == 0)
   {
      colour.r = 0;
      colour.g = 229;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"turquoise3",10) == 0)
   {
      colour.r = 0;
      colour.g = 197;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"turquoise4",10) == 0)
   {
      colour.r = 0;
      colour.g = 134;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"cyan1",5) == 0)
   {
      colour.r = 0;
      colour.g = 255;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"cyan2",5) == 0)
   {
      colour.r = 0;
      colour.g = 238;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"cyan3",5) == 0)
   {
      colour.r = 0;
      colour.g = 205;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"cyan4",5) == 0)
   {
      colour.r = 0;
      colour.g = 139;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"DarkSlateGray1",14) == 0)
   {
      colour.r = 151;
      colour.g = 255;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"DarkSlateGray2",14) == 0)
   {
      colour.r = 141;
      colour.g = 238;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"DarkSlateGray3",14) == 0)
   {
      colour.r = 121;
      colour.g = 205;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"DarkSlateGray4",14) == 0)
   {
      colour.r = 82;
      colour.g = 139;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"aquamarine1",11) == 0)
   {
      colour.r = 127;
      colour.g = 255;
      colour.b = 212;
   }
   else if (strncasecmp(x11ColourName,"aquamarine2",11) == 0)
   {
      colour.r = 118;
      colour.g = 238;
      colour.b = 198;
   }
   else if (strncasecmp(x11ColourName,"aquamarine3",11) == 0)
   {
      colour.r = 102;
      colour.g = 205;
      colour.b = 170;
   }
   else if (strncasecmp(x11ColourName,"aquamarine4",11) == 0)
   {
      colour.r = 69;
      colour.g = 139;
      colour.b = 116;
   }
   else if (strncasecmp(x11ColourName,"DarkSeaGreen1",13) == 0)
   {
      colour.r = 193;
      colour.g = 255;
      colour.b = 193;
   }
   else if (strncasecmp(x11ColourName,"DarkSeaGreen2",13) == 0)
   {
      colour.r = 180;
      colour.g = 238;
      colour.b = 180;
   }
   else if (strncasecmp(x11ColourName,"DarkSeaGreen3",13) == 0)
   {
      colour.r = 155;
      colour.g = 205;
      colour.b = 155;
   }
   else if (strncasecmp(x11ColourName,"DarkSeaGreen4",13) == 0)
   {
      colour.r = 105;
      colour.g = 139;
      colour.b = 105;
   }
   else if (strncasecmp(x11ColourName,"SeaGreen1",9) == 0)
   {
      colour.r = 84;
      colour.g = 255;
      colour.b = 159;
   }
   else if (strncasecmp(x11ColourName,"SeaGreen2",9) == 0)
   {
      colour.r = 78;
      colour.g = 238;
      colour.b = 148;
   }
   else if (strncasecmp(x11ColourName,"SeaGreen3",9) == 0)
   {
      colour.r = 67;
      colour.g = 205;
      colour.b = 128;
   }
   else if (strncasecmp(x11ColourName,"SeaGreen4",9) == 0)
   {
      colour.r = 46;
      colour.g = 139;
      colour.b = 87;
   }
   else if (strncasecmp(x11ColourName,"PaleGreen1",10) == 0)
   {
      colour.r = 154;
      colour.g = 255;
      colour.b = 154;
   }
   else if (strncasecmp(x11ColourName,"PaleGreen2",10) == 0)
   {
      colour.r = 144;
      colour.g = 238;
      colour.b = 144;
   }
   else if (strncasecmp(x11ColourName,"PaleGreen3",10) == 0)
   {
      colour.r = 124;
      colour.g = 205;
      colour.b = 124;
   }
   else if (strncasecmp(x11ColourName,"PaleGreen4",10) == 0)
   {
      colour.r = 84;
      colour.g = 139;
      colour.b = 84;
   }
   else if (strncasecmp(x11ColourName,"SpringGreen1",12) == 0)
   {
      colour.r = 0;
      colour.g = 255;
      colour.b = 127;
   }
   else if (strncasecmp(x11ColourName,"SpringGreen2",12) == 0)
   {
      colour.r = 0;
      colour.g = 238;
      colour.b = 118;
   }
   else if (strncasecmp(x11ColourName,"SpringGreen3",12) == 0)
   {
      colour.r = 0;
      colour.g = 205;
      colour.b = 102;
   }
   else if (strncasecmp(x11ColourName,"SpringGreen4",12) == 0)
   {
      colour.r = 0;
      colour.g = 139;
      colour.b = 69;
   }
   else if (strncasecmp(x11ColourName,"green1",6) == 0)
   {
      colour.r = 0;
      colour.g = 255;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"green2",6) == 0)
   {
      colour.r = 0;
      colour.g = 238;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"green3",6) == 0)
   {
      colour.r = 0;
      colour.g = 205;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"green4",6) == 0)
   {
      colour.r = 0;
      colour.g = 139;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"chartreuse1",11) == 0)
   {
      colour.r = 127;
      colour.g = 255;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"chartreuse2",11) == 0)
   {
      colour.r = 118;
      colour.g = 238;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"chartreuse3",11) == 0)
   {
      colour.r = 102;
      colour.g = 205;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"chartreuse4",11) == 0)
   {
      colour.r = 69;
      colour.g = 139;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"OliveDrab1",10) == 0)
   {
      colour.r = 192;
      colour.g = 255;
      colour.b = 62;
   }
   else if (strncasecmp(x11ColourName,"OliveDrab2",10) == 0)
   {
      colour.r = 179;
      colour.g = 238;
      colour.b = 58;
   }
   else if (strncasecmp(x11ColourName,"OliveDrab3",10) == 0)
   {
      colour.r = 154;
      colour.g = 205;
      colour.b = 50;
   }
   else if (strncasecmp(x11ColourName,"OliveDrab4",10) == 0)
   {
      colour.r = 105;
      colour.g = 139;
      colour.b = 34;
   }
   else if (strncasecmp(x11ColourName,"DarkOliveGreen1",15) == 0)
   {
      colour.r = 202;
      colour.g = 255;
      colour.b = 112;
   }
   else if (strncasecmp(x11ColourName,"DarkOliveGreen2",15) == 0)
   {
      colour.r = 188;
      colour.g = 238;
      colour.b = 104;
   }
   else if (strncasecmp(x11ColourName,"DarkOliveGreen3",15) == 0)
   {
      colour.r = 162;
      colour.g = 205;
      colour.b = 90;
   }
   else if (strncasecmp(x11ColourName,"DarkOliveGreen4",15) == 0)
   {
      colour.r = 110;
      colour.g = 139;
      colour.b = 61;
   }
   else if (strncasecmp(x11ColourName,"khaki1",6) == 0)
   {
      colour.r = 255;
      colour.g = 246;
      colour.b = 143;
   }
   else if (strncasecmp(x11ColourName,"khaki2",6) == 0)
   {
      colour.r = 238;
      colour.g = 230;
      colour.b = 133;
   }
   else if (strncasecmp(x11ColourName,"khaki3",6) == 0)
   {
      colour.r = 205;
      colour.g = 198;
      colour.b = 115;
   }
   else if (strncasecmp(x11ColourName,"khaki4",6) == 0)
   {
      colour.r = 139;
      colour.g = 134;
      colour.b = 78;
   }
   else if (strncasecmp(x11ColourName,"LightGoldenrod1",15) == 0)
   {
      colour.r = 255;
      colour.g = 236;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"LightGoldenrod2",15) == 0)
   {
      colour.r = 238;
      colour.g = 220;
      colour.b = 130;
   }
   else if (strncasecmp(x11ColourName,"LightGoldenrod3",15) == 0)
   {
      colour.r = 205;
      colour.g = 190;
      colour.b = 112;
   }
   else if (strncasecmp(x11ColourName,"LightGoldenrod4",15) == 0)
   {
      colour.r = 139;
      colour.g = 129;
      colour.b = 76;
   }
   else if (strncasecmp(x11ColourName,"LightYellow1",12) == 0)
   {
      colour.r = 255;
      colour.g = 255;
      colour.b = 224;
   }
   else if (strncasecmp(x11ColourName,"LightYellow2",12) == 0)
   {
      colour.r = 238;
      colour.g = 238;
      colour.b = 209;
   }
   else if (strncasecmp(x11ColourName,"LightYellow3",12) == 0)
   {
      colour.r = 205;
      colour.g = 205;
      colour.b = 180;
   }
   else if (strncasecmp(x11ColourName,"LightYellow4",12) == 0)
   {
      colour.r = 139;
      colour.g = 139;
      colour.b = 122;
   }
   else if (strncasecmp(x11ColourName,"yellow1",7) == 0)
   {
      colour.r = 255;
      colour.g = 255;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"yellow2",7) == 0)
   {
      colour.r = 238;
      colour.g = 238;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"yellow3",7) == 0)
   {
      colour.r = 205;
      colour.g = 205;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"yellow4",7) == 0)
   {
      colour.r = 139;
      colour.g = 139;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"gold1",5) == 0)
   {
      colour.r = 255;
      colour.g = 215;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"gold2",5) == 0)
   {
      colour.r = 238;
      colour.g = 201;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"gold3",5) == 0)
   {
      colour.r = 205;
      colour.g = 173;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"gold4",5) == 0)
   {
      colour.r = 139;
      colour.g = 117;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"goldenrod1",10) == 0)
   {
      colour.r = 255;
      colour.g = 193;
      colour.b = 37;
   }
   else if (strncasecmp(x11ColourName,"goldenrod2",10) == 0)
   {
      colour.r = 238;
      colour.g = 180;
      colour.b = 34;
   }
   else if (strncasecmp(x11ColourName,"goldenrod3",10) == 0)
   {
      colour.r = 205;
      colour.g = 155;
      colour.b = 29;
   }
   else if (strncasecmp(x11ColourName,"goldenrod4",10) == 0)
   {
      colour.r = 139;
      colour.g = 105;
      colour.b = 20;
   }
   else if (strncasecmp(x11ColourName,"DarkGoldenrod1",14) == 0)
   {
      colour.r = 255;
      colour.g = 185;
      colour.b = 15;
   }
   else if (strncasecmp(x11ColourName,"DarkGoldenrod2",14) == 0)
   {
      colour.r = 238;
      colour.g = 173;
      colour.b = 14;
   }
   else if (strncasecmp(x11ColourName,"DarkGoldenrod3",14) == 0)
   {
      colour.r = 205;
      colour.g = 149;
      colour.b = 12;
   }
   else if (strncasecmp(x11ColourName,"DarkGoldenrod4",14) == 0)
   {
      colour.r = 139;
      colour.g = 101;
      colour.b = 8;
   }
   else if (strncasecmp(x11ColourName,"RosyBrown1",10) == 0)
   {
      colour.r = 255;
      colour.g = 193;
      colour.b = 193;
   }
   else if (strncasecmp(x11ColourName,"RosyBrown2",10) == 0)
   {
      colour.r = 238;
      colour.g = 180;
      colour.b = 180;
   }
   else if (strncasecmp(x11ColourName,"RosyBrown3",10) == 0)
   {
      colour.r = 205;
      colour.g = 155;
      colour.b = 155;
   }
   else if (strncasecmp(x11ColourName,"RosyBrown4",10) == 0)
   {
      colour.r = 139;
      colour.g = 105;
      colour.b = 105;
   }
   else if (strncasecmp(x11ColourName,"IndianRed1",10) == 0)
   {
      colour.r = 255;
      colour.g = 106;
      colour.b = 106;
   }
   else if (strncasecmp(x11ColourName,"IndianRed2",10) == 0)
   {
      colour.r = 238;
      colour.g = 99;
      colour.b = 99;
   }
   else if (strncasecmp(x11ColourName,"IndianRed3",10) == 0)
   {
      colour.r = 205;
      colour.g = 85;
      colour.b = 85;
   }
   else if (strncasecmp(x11ColourName,"IndianRed4",10) == 0)
   {
      colour.r = 139;
      colour.g = 58;
      colour.b = 58;
   }
   else if (strncasecmp(x11ColourName,"sienna1",7) == 0)
   {
      colour.r = 255;
      colour.g = 130;
      colour.b = 71;
   }
   else if (strncasecmp(x11ColourName,"sienna2",7) == 0)
   {
      colour.r = 238;
      colour.g = 121;
      colour.b = 66;
   }
   else if (strncasecmp(x11ColourName,"sienna3",7) == 0)
   {
      colour.r = 205;
      colour.g = 104;
      colour.b = 57;
   }
   else if (strncasecmp(x11ColourName,"sienna4",7) == 0)
   {
      colour.r = 139;
      colour.g = 71;
      colour.b = 38;
   }
   else if (strncasecmp(x11ColourName,"burlywood1",10) == 0)
   {
      colour.r = 255;
      colour.g = 211;
      colour.b = 155;
   }
   else if (strncasecmp(x11ColourName,"burlywood2",10) == 0)
   {
      colour.r = 238;
      colour.g = 197;
      colour.b = 145;
   }
   else if (strncasecmp(x11ColourName,"burlywood3",10) == 0)
   {
      colour.r = 205;
      colour.g = 170;
      colour.b = 125;
   }
   else if (strncasecmp(x11ColourName,"burlywood4",10) == 0)
   {
      colour.r = 139;
      colour.g = 115;
      colour.b = 85;
   }
   else if (strncasecmp(x11ColourName,"wheat1",6) == 0)
   {
      colour.r = 255;
      colour.g = 231;
      colour.b = 186;
   }
   else if (strncasecmp(x11ColourName,"wheat2",6) == 0)
   {
      colour.r = 238;
      colour.g = 216;
      colour.b = 174;
   }
   else if (strncasecmp(x11ColourName,"wheat3",6) == 0)
   {
      colour.r = 205;
      colour.g = 186;
      colour.b = 150;
   }
   else if (strncasecmp(x11ColourName,"wheat4",6) == 0)
   {
      colour.r = 139;
      colour.g = 126;
      colour.b = 102;
   }
   else if (strncasecmp(x11ColourName,"tan1",4) == 0)
   {
      colour.r = 255;
      colour.g = 165;
      colour.b = 79;
   }
   else if (strncasecmp(x11ColourName,"tan2",4) == 0)
   {
      colour.r = 238;
      colour.g = 154;
      colour.b = 73;
   }
   else if (strncasecmp(x11ColourName,"tan3",4) == 0)
   {
      colour.r = 205;
      colour.g = 133;
      colour.b = 63;
   }
   else if (strncasecmp(x11ColourName,"tan4",4) == 0)
   {
      colour.r = 139;
      colour.g = 90;
      colour.b = 43;
   }
   else if (strncasecmp(x11ColourName,"chocolate1",10) == 0)
   {
      colour.r = 255;
      colour.g = 127;
      colour.b = 36;
   }
   else if (strncasecmp(x11ColourName,"chocolate2",10) == 0)
   {
      colour.r = 238;
      colour.g = 118;
      colour.b = 33;
   }
   else if (strncasecmp(x11ColourName,"chocolate3",10) == 0)
   {
      colour.r = 205;
      colour.g = 102;
      colour.b = 29;
   }
   else if (strncasecmp(x11ColourName,"chocolate4",10) == 0)
   {
      colour.r = 139;
      colour.g = 69;
      colour.b = 19;
   }
   else if (strncasecmp(x11ColourName,"firebrick1",10) == 0)
   {
      colour.r = 255;
      colour.g = 48;
      colour.b = 48;
   }
   else if (strncasecmp(x11ColourName,"firebrick2",10) == 0)
   {
      colour.r = 238;
      colour.g = 44;
      colour.b = 44;
   }
   else if (strncasecmp(x11ColourName,"firebrick3",10) == 0)
   {
      colour.r = 205;
      colour.g = 38;
      colour.b = 38;
   }
   else if (strncasecmp(x11ColourName,"firebrick4",10) == 0)
   {
      colour.r = 139;
      colour.g = 26;
      colour.b = 26;
   }
   else if (strncasecmp(x11ColourName,"brown1",6) == 0)
   {
      colour.r = 255;
      colour.g = 64;
      colour.b = 64;
   }
   else if (strncasecmp(x11ColourName,"brown2",6) == 0)
   {
      colour.r = 238;
      colour.g = 59;
      colour.b = 59;
   }
   else if (strncasecmp(x11ColourName,"brown3",6) == 0)
   {
      colour.r = 205;
      colour.g = 51;
      colour.b = 51;
   }
   else if (strncasecmp(x11ColourName,"brown4",6) == 0)
   {
      colour.r = 139;
      colour.g = 35;
      colour.b = 35;
   }
   else if (strncasecmp(x11ColourName,"salmon1",7) == 0)
   {
      colour.r = 255;
      colour.g = 140;
      colour.b = 105;
   }
   else if (strncasecmp(x11ColourName,"salmon2",7) == 0)
   {
      colour.r = 238;
      colour.g = 130;
      colour.b = 98;
   }
   else if (strncasecmp(x11ColourName,"salmon3",7) == 0)
   {
      colour.r = 205;
      colour.g = 112;
      colour.b = 84;
   }
   else if (strncasecmp(x11ColourName,"salmon4",7) == 0)
   {
      colour.r = 139;
      colour.g = 76;
      colour.b = 57;
   }
   else if (strncasecmp(x11ColourName,"LightSalmon1",12) == 0)
   {
      colour.r = 255;
      colour.g = 160;
      colour.b = 122;
   }
   else if (strncasecmp(x11ColourName,"LightSalmon2",12) == 0)
   {
      colour.r = 238;
      colour.g = 149;
      colour.b = 114;
   }
   else if (strncasecmp(x11ColourName,"LightSalmon3",12) == 0)
   {
      colour.r = 205;
      colour.g = 129;
      colour.b = 98;
   }
   else if (strncasecmp(x11ColourName,"LightSalmon4",12) == 0)
   {
      colour.r = 139;
      colour.g = 87;
      colour.b = 66;
   }
   else if (strncasecmp(x11ColourName,"orange1",7) == 0)
   {
      colour.r = 255;
      colour.g = 165;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"orange2",7) == 0)
   {
      colour.r = 238;
      colour.g = 154;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"orange3",7) == 0)
   {
      colour.r = 205;
      colour.g = 133;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"orange4",7) == 0)
   {
      colour.r = 139;
      colour.g = 90;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"DarkOrange1",11) == 0)
   {
      colour.r = 255;
      colour.g = 127;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"DarkOrange2",11) == 0)
   {
      colour.r = 238;
      colour.g = 118;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"DarkOrange3",11) == 0)
   {
      colour.r = 205;
      colour.g = 102;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"DarkOrange4",11) == 0)
   {
      colour.r = 139;
      colour.g = 69;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"coral1",6) == 0)
   {
      colour.r = 255;
      colour.g = 114;
      colour.b = 86;
   }
   else if (strncasecmp(x11ColourName,"coral2",6) == 0)
   {
      colour.r = 238;
      colour.g = 106;
      colour.b = 80;
   }
   else if (strncasecmp(x11ColourName,"coral3",6) == 0)
   {
      colour.r = 205;
      colour.g = 91;
      colour.b = 69;
   }
   else if (strncasecmp(x11ColourName,"coral4",6) == 0)
   {
      colour.r = 139;
      colour.g = 62;
      colour.b = 47;
   }
   else if (strncasecmp(x11ColourName,"tomato1",7) == 0)
   {
      colour.r = 255;
      colour.g = 99;
      colour.b = 71;
   }
   else if (strncasecmp(x11ColourName,"tomato2",7) == 0)
   {
      colour.r = 238;
      colour.g = 92;
      colour.b = 66;
   }
   else if (strncasecmp(x11ColourName,"tomato3",7) == 0)
   {
      colour.r = 205;
      colour.g = 79;
      colour.b = 57;
   }
   else if (strncasecmp(x11ColourName,"tomato4",7) == 0)
   {
      colour.r = 139;
      colour.g = 54;
      colour.b = 38;
   }
   else if (strncasecmp(x11ColourName,"OrangeRed1",10) == 0)
   {
      colour.r = 255;
      colour.g = 69;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"OrangeRed2",10) == 0)
   {
      colour.r = 238;
      colour.g = 64;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"OrangeRed3",10) == 0)
   {
      colour.r = 205;
      colour.g = 55;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"OrangeRed4",10) == 0)
   {
      colour.r = 139;
      colour.g = 37;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"red1",4) == 0)
   {
      colour.r = 255;
      colour.g = 0;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"red2",4) == 0)
   {
      colour.r = 238;
      colour.g = 0;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"red3",4) == 0)
   {
      colour.r = 205;
      colour.g = 0;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"red4",4) == 0)
   {
      colour.r = 139;
      colour.g = 0;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"DeepPink1",9) == 0)
   {
      colour.r = 255;
      colour.g = 20;
      colour.b = 147;
   }
   else if (strncasecmp(x11ColourName,"DeepPink2",9) == 0)
   {
      colour.r = 238;
      colour.g = 18;
      colour.b = 137;
   }
   else if (strncasecmp(x11ColourName,"DeepPink3",9) == 0)
   {
      colour.r = 205;
      colour.g = 16;
      colour.b = 118;
   }
   else if (strncasecmp(x11ColourName,"DeepPink4",9) == 0)
   {
      colour.r = 139;
      colour.g = 10;
      colour.b = 80;
   }
   else if (strncasecmp(x11ColourName,"HotPink1",8) == 0)
   {
      colour.r = 255;
      colour.g = 110;
      colour.b = 180;
   }
   else if (strncasecmp(x11ColourName,"HotPink2",8) == 0)
   {
      colour.r = 238;
      colour.g = 106;
      colour.b = 167;
   }
   else if (strncasecmp(x11ColourName,"HotPink3",8) == 0)
   {
      colour.r = 205;
      colour.g = 96;
      colour.b = 144;
   }
   else if (strncasecmp(x11ColourName,"HotPink4",8) == 0)
   {
      colour.r = 139;
      colour.g = 58;
      colour.b = 98;
   }
   else if (strncasecmp(x11ColourName,"pink1",5) == 0)
   {
      colour.r = 255;
      colour.g = 181;
      colour.b = 197;
   }
   else if (strncasecmp(x11ColourName,"pink2",5) == 0)
   {
      colour.r = 238;
      colour.g = 169;
      colour.b = 184;
   }
   else if (strncasecmp(x11ColourName,"pink3",5) == 0)
   {
      colour.r = 205;
      colour.g = 145;
      colour.b = 158;
   }
   else if (strncasecmp(x11ColourName,"pink4",5) == 0)
   {
      colour.r = 139;
      colour.g = 99;
      colour.b = 108;
   }
   else if (strncasecmp(x11ColourName,"LightPink1",10) == 0)
   {
      colour.r = 255;
      colour.g = 174;
      colour.b = 185;
   }
   else if (strncasecmp(x11ColourName,"LightPink2",10) == 0)
   {
      colour.r = 238;
      colour.g = 162;
      colour.b = 173;
   }
   else if (strncasecmp(x11ColourName,"LightPink3",10) == 0)
   {
      colour.r = 205;
      colour.g = 140;
      colour.b = 149;
   }
   else if (strncasecmp(x11ColourName,"LightPink4",10) == 0)
   {
      colour.r = 139;
      colour.g = 95;
      colour.b = 101;
   }
   else if (strncasecmp(x11ColourName,"PaleVioletRed1",14) == 0)
   {
      colour.r = 255;
      colour.g = 130;
      colour.b = 171;
   }
   else if (strncasecmp(x11ColourName,"PaleVioletRed2",14) == 0)
   {
      colour.r = 238;
      colour.g = 121;
      colour.b = 159;
   }
   else if (strncasecmp(x11ColourName,"PaleVioletRed3",14) == 0)
   {
      colour.r = 205;
      colour.g = 104;
      colour.b = 137;
   }
   else if (strncasecmp(x11ColourName,"PaleVioletRed4",14) == 0)
   {
      colour.r = 139;
      colour.g = 71;
      colour.b = 93;
   }
   else if (strncasecmp(x11ColourName,"maroon1",7) == 0)
   {
      colour.r = 255;
      colour.g = 52;
      colour.b = 179;
   }
   else if (strncasecmp(x11ColourName,"maroon2",7) == 0)
   {
      colour.r = 238;
      colour.g = 48;
      colour.b = 167;
   }
   else if (strncasecmp(x11ColourName,"maroon3",7) == 0)
   {
      colour.r = 205;
      colour.g = 41;
      colour.b = 144;
   }
   else if (strncasecmp(x11ColourName,"maroon4",7) == 0)
   {
      colour.r = 139;
      colour.g = 28;
      colour.b = 98;
   }
   else if (strncasecmp(x11ColourName,"VioletRed1",10) == 0)
   {
      colour.r = 255;
      colour.g = 62;
      colour.b = 150;
   }
   else if (strncasecmp(x11ColourName,"VioletRed2",10) == 0)
   {
      colour.r = 238;
      colour.g = 58;
      colour.b = 140;
   }
   else if (strncasecmp(x11ColourName,"VioletRed3",10) == 0)
   {
      colour.r = 205;
      colour.g = 50;
      colour.b = 120;
   }
   else if (strncasecmp(x11ColourName,"VioletRed4",10) == 0)
   {
      colour.r = 139;
      colour.g = 34;
      colour.b = 82;
   }
   else if (strncasecmp(x11ColourName,"magenta1",8) == 0)
   {
      colour.r = 255;
      colour.g = 0;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"magenta2",8) == 0)
   {
      colour.r = 238;
      colour.g = 0;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"magenta3",8) == 0)
   {
      colour.r = 205;
      colour.g = 0;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"magenta4",8) == 0)
   {
      colour.r = 139;
      colour.g = 0;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"orchid1",7) == 0)
   {
      colour.r = 255;
      colour.g = 131;
      colour.b = 250;
   }
   else if (strncasecmp(x11ColourName,"orchid2",7) == 0)
   {
      colour.r = 238;
      colour.g = 122;
      colour.b = 233;
   }
   else if (strncasecmp(x11ColourName,"orchid3",7) == 0)
   {
      colour.r = 205;
      colour.g = 105;
      colour.b = 201;
   }
   else if (strncasecmp(x11ColourName,"orchid4",7) == 0)
   {
      colour.r = 139;
      colour.g = 71;
      colour.b = 137;
   }
   else if (strncasecmp(x11ColourName,"plum1",5) == 0)
   {
      colour.r = 255;
      colour.g = 187;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"plum2",5) == 0)
   {
      colour.r = 238;
      colour.g = 174;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"plum3",5) == 0)
   {
      colour.r = 205;
      colour.g = 150;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"plum4",5) == 0)
   {
      colour.r = 139;
      colour.g = 102;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"MediumOrchid1",13) == 0)
   {
      colour.r = 224;
      colour.g = 102;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"MediumOrchid2",13) == 0)
   {
      colour.r = 209;
      colour.g = 95;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"MediumOrchid3",13) == 0)
   {
      colour.r = 180;
      colour.g = 82;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"MediumOrchid4",13) == 0)
   {
      colour.r = 122;
      colour.g = 55;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"DarkOrchid1",11) == 0)
   {
      colour.r = 191;
      colour.g = 62;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"DarkOrchid2",11) == 0)
   {
      colour.r = 178;
      colour.g = 58;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"DarkOrchid3",11) == 0)
   {
      colour.r = 154;
      colour.g = 50;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"DarkOrchid4",11) == 0)
   {
      colour.r = 104;
      colour.g = 34;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"purple1",7) == 0)
   {
      colour.r = 155;
      colour.g = 48;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"purple2",7) == 0)
   {
      colour.r = 145;
      colour.g = 44;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"purple3",7) == 0)
   {
      colour.r = 125;
      colour.g = 38;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"purple4",7) == 0)
   {
      colour.r = 85;
      colour.g = 26;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"MediumPurple1",13) == 0)
   {
      colour.r = 171;
      colour.g = 130;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"MediumPurple2",13) == 0)
   {
      colour.r = 159;
      colour.g = 121;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"MediumPurple3",13) == 0)
   {
      colour.r = 137;
      colour.g = 104;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"MediumPurple4",13) == 0)
   {
      colour.r = 93;
      colour.g = 71;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"thistle1",8) == 0)
   {
      colour.r = 255;
      colour.g = 225;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"thistle2",8) == 0)
   {
      colour.r = 238;
      colour.g = 210;
      colour.b = 238;
   }
   else if (strncasecmp(x11ColourName,"thistle3",8) == 0)
   {
      colour.r = 205;
      colour.g = 181;
      colour.b = 205;
   }
   else if (strncasecmp(x11ColourName,"thistle4",8) == 0)
   {
      colour.r = 139;
      colour.g = 123;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"gray0",5) == 0)
   {
      colour.r = 0;
      colour.g = 0;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"grey0",5) == 0)
   {
      colour.r = 0;
      colour.g = 0;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"gray1",5) == 0)
   {
      colour.r = 3;
      colour.g = 3;
      colour.b = 3;
   }
   else if (strncasecmp(x11ColourName,"grey1",5) == 0)
   {
      colour.r = 3;
      colour.g = 3;
      colour.b = 3;
   }
   else if (strncasecmp(x11ColourName,"gray2",5) == 0)
   {
      colour.r = 5;
      colour.g = 5;
      colour.b = 5;
   }
   else if (strncasecmp(x11ColourName,"grey2",5) == 0)
   {
      colour.r = 5;
      colour.g = 5;
      colour.b = 5;
   }
   else if (strncasecmp(x11ColourName,"gray3",5) == 0)
   {
      colour.r = 8;
      colour.g = 8;
      colour.b = 8;
   }
   else if (strncasecmp(x11ColourName,"grey3",5) == 0)
   {
      colour.r = 8;
      colour.g = 8;
      colour.b = 8;
   }
   else if (strncasecmp(x11ColourName,"gray4",5) == 0)
   {
      colour.r = 10;
      colour.g = 10;
      colour.b = 10;
   }
   else if (strncasecmp(x11ColourName,"grey4",5) == 0)
   {
      colour.r = 10;
      colour.g = 10;
      colour.b = 10;
   }
   else if (strncasecmp(x11ColourName,"gray5",5) == 0)
   {
      colour.r = 13;
      colour.g = 13;
      colour.b = 13;
   }
   else if (strncasecmp(x11ColourName,"grey5",5) == 0)
   {
      colour.r = 13;
      colour.g = 13;
      colour.b = 13;
   }
   else if (strncasecmp(x11ColourName,"gray6",5) == 0)
   {
      colour.r = 15;
      colour.g = 15;
      colour.b = 15;
   }
   else if (strncasecmp(x11ColourName,"grey6",5) == 0)
   {
      colour.r = 15;
      colour.g = 15;
      colour.b = 15;
   }
   else if (strncasecmp(x11ColourName,"gray7",5) == 0)
   {
      colour.r = 18;
      colour.g = 18;
      colour.b = 18;
   }
   else if (strncasecmp(x11ColourName,"grey7",5) == 0)
   {
      colour.r = 18;
      colour.g = 18;
      colour.b = 18;
   }
   else if (strncasecmp(x11ColourName,"gray8",5) == 0)
   {
      colour.r = 20;
      colour.g = 20;
      colour.b = 20;
   }
   else if (strncasecmp(x11ColourName,"grey8",5) == 0)
   {
      colour.r = 20;
      colour.g = 20;
      colour.b = 20;
   }
   else if (strncasecmp(x11ColourName,"gray9",5) == 0)
   {
      colour.r = 23;
      colour.g = 23;
      colour.b = 23;
   }
   else if (strncasecmp(x11ColourName,"grey9",5) == 0)
   {
      colour.r = 23;
      colour.g = 23;
      colour.b = 23;
   }
   else if (strncasecmp(x11ColourName,"gray10",6) == 0)
   {
      colour.r = 26;
      colour.g = 26;
      colour.b = 26;
   }
   else if (strncasecmp(x11ColourName,"grey10",6) == 0)
   {
      colour.r = 26;
      colour.g = 26;
      colour.b = 26;
   }
   else if (strncasecmp(x11ColourName,"gray11",6) == 0)
   {
      colour.r = 28;
      colour.g = 28;
      colour.b = 28;
   }
   else if (strncasecmp(x11ColourName,"grey11",6) == 0)
   {
      colour.r = 28;
      colour.g = 28;
      colour.b = 28;
   }
   else if (strncasecmp(x11ColourName,"gray12",6) == 0)
   {
      colour.r = 31;
      colour.g = 31;
      colour.b = 31;
   }
   else if (strncasecmp(x11ColourName,"grey12",6) == 0)
   {
      colour.r = 31;
      colour.g = 31;
      colour.b = 31;
   }
   else if (strncasecmp(x11ColourName,"gray13",6) == 0)
   {
      colour.r = 33;
      colour.g = 33;
      colour.b = 33;
   }
   else if (strncasecmp(x11ColourName,"grey13",6) == 0)
   {
      colour.r = 33;
      colour.g = 33;
      colour.b = 33;
   }
   else if (strncasecmp(x11ColourName,"gray14",6) == 0)
   {
      colour.r = 36;
      colour.g = 36;
      colour.b = 36;
   }
   else if (strncasecmp(x11ColourName,"grey14",6) == 0)
   {
      colour.r = 36;
      colour.g = 36;
      colour.b = 36;
   }
   else if (strncasecmp(x11ColourName,"gray15",6) == 0)
   {
      colour.r = 38;
      colour.g = 38;
      colour.b = 38;
   }
   else if (strncasecmp(x11ColourName,"grey15",6) == 0)
   {
      colour.r = 38;
      colour.g = 38;
      colour.b = 38;
   }
   else if (strncasecmp(x11ColourName,"gray16",6) == 0)
   {
      colour.r = 41;
      colour.g = 41;
      colour.b = 41;
   }
   else if (strncasecmp(x11ColourName,"grey16",6) == 0)
   {
      colour.r = 41;
      colour.g = 41;
      colour.b = 41;
   }
   else if (strncasecmp(x11ColourName,"gray17",6) == 0)
   {
      colour.r = 43;
      colour.g = 43;
      colour.b = 43;
   }
   else if (strncasecmp(x11ColourName,"grey17",6) == 0)
   {
      colour.r = 43;
      colour.g = 43;
      colour.b = 43;
   }
   else if (strncasecmp(x11ColourName,"gray18",6) == 0)
   {
      colour.r = 46;
      colour.g = 46;
      colour.b = 46;
   }
   else if (strncasecmp(x11ColourName,"grey18",6) == 0)
   {
      colour.r = 46;
      colour.g = 46;
      colour.b = 46;
   }
   else if (strncasecmp(x11ColourName,"gray19",6) == 0)
   {
      colour.r = 48;
      colour.g = 48;
      colour.b = 48;
   }
   else if (strncasecmp(x11ColourName,"grey19",6) == 0)
   {
      colour.r = 48;
      colour.g = 48;
      colour.b = 48;
   }
   else if (strncasecmp(x11ColourName,"gray20",6) == 0)
   {
      colour.r = 51;
      colour.g = 51;
      colour.b = 51;
   }
   else if (strncasecmp(x11ColourName,"grey20",6) == 0)
   {
      colour.r = 51;
      colour.g = 51;
      colour.b = 51;
   }
   else if (strncasecmp(x11ColourName,"gray21",6) == 0)
   {
      colour.r = 54;
      colour.g = 54;
      colour.b = 54;
   }
   else if (strncasecmp(x11ColourName,"grey21",6) == 0)
   {
      colour.r = 54;
      colour.g = 54;
      colour.b = 54;
   }
   else if (strncasecmp(x11ColourName,"gray22",6) == 0)
   {
      colour.r = 56;
      colour.g = 56;
      colour.b = 56;
   }
   else if (strncasecmp(x11ColourName,"grey22",6) == 0)
   {
      colour.r = 56;
      colour.g = 56;
      colour.b = 56;
   }
   else if (strncasecmp(x11ColourName,"gray23",6) == 0)
   {
      colour.r = 59;
      colour.g = 59;
      colour.b = 59;
   }
   else if (strncasecmp(x11ColourName,"grey23",6) == 0)
   {
      colour.r = 59;
      colour.g = 59;
      colour.b = 59;
   }
   else if (strncasecmp(x11ColourName,"gray24",6) == 0)
   {
      colour.r = 61;
      colour.g = 61;
      colour.b = 61;
   }
   else if (strncasecmp(x11ColourName,"grey24",6) == 0)
   {
      colour.r = 61;
      colour.g = 61;
      colour.b = 61;
   }
   else if (strncasecmp(x11ColourName,"gray25",6) == 0)
   {
      colour.r = 64;
      colour.g = 64;
      colour.b = 64;
   }
   else if (strncasecmp(x11ColourName,"grey25",6) == 0)
   {
      colour.r = 64;
      colour.g = 64;
      colour.b = 64;
   }
   else if (strncasecmp(x11ColourName,"gray26",6) == 0)
   {
      colour.r = 66;
      colour.g = 66;
      colour.b = 66;
   }
   else if (strncasecmp(x11ColourName,"grey26",6) == 0)
   {
      colour.r = 66;
      colour.g = 66;
      colour.b = 66;
   }
   else if (strncasecmp(x11ColourName,"gray27",6) == 0)
   {
      colour.r = 69;
      colour.g = 69;
      colour.b = 69;
   }
   else if (strncasecmp(x11ColourName,"grey27",6) == 0)
   {
      colour.r = 69;
      colour.g = 69;
      colour.b = 69;
   }
   else if (strncasecmp(x11ColourName,"gray28",6) == 0)
   {
      colour.r = 71;
      colour.g = 71;
      colour.b = 71;
   }
   else if (strncasecmp(x11ColourName,"grey28",6) == 0)
   {
      colour.r = 71;
      colour.g = 71;
      colour.b = 71;
   }
   else if (strncasecmp(x11ColourName,"gray29",6) == 0)
   {
      colour.r = 74;
      colour.g = 74;
      colour.b = 74;
   }
   else if (strncasecmp(x11ColourName,"grey29",6) == 0)
   {
      colour.r = 74;
      colour.g = 74;
      colour.b = 74;
   }
   else if (strncasecmp(x11ColourName,"gray30",6) == 0)
   {
      colour.r = 77;
      colour.g = 77;
      colour.b = 77;
   }
   else if (strncasecmp(x11ColourName,"grey30",6) == 0)
   {
      colour.r = 77;
      colour.g = 77;
      colour.b = 77;
   }
   else if (strncasecmp(x11ColourName,"gray31",6) == 0)
   {
      colour.r = 79;
      colour.g = 79;
      colour.b = 79;
   }
   else if (strncasecmp(x11ColourName,"grey31",6) == 0)
   {
      colour.r = 79;
      colour.g = 79;
      colour.b = 79;
   }
   else if (strncasecmp(x11ColourName,"gray32",6) == 0)
   {
      colour.r = 82;
      colour.g = 82;
      colour.b = 82;
   }
   else if (strncasecmp(x11ColourName,"grey32",6) == 0)
   {
      colour.r = 82;
      colour.g = 82;
      colour.b = 82;
   }
   else if (strncasecmp(x11ColourName,"gray33",6) == 0)
   {
      colour.r = 84;
      colour.g = 84;
      colour.b = 84;
   }
   else if (strncasecmp(x11ColourName,"grey33",6) == 0)
   {
      colour.r = 84;
      colour.g = 84;
      colour.b = 84;
   }
   else if (strncasecmp(x11ColourName,"gray34",6) == 0)
   {
      colour.r = 87;
      colour.g = 87;
      colour.b = 87;
   }
   else if (strncasecmp(x11ColourName,"grey34",6) == 0)
   {
      colour.r = 87;
      colour.g = 87;
      colour.b = 87;
   }
   else if (strncasecmp(x11ColourName,"gray35",6) == 0)
   {
      colour.r = 89;
      colour.g = 89;
      colour.b = 89;
   }
   else if (strncasecmp(x11ColourName,"grey35",6) == 0)
   {
      colour.r = 89;
      colour.g = 89;
      colour.b = 89;
   }
   else if (strncasecmp(x11ColourName,"gray36",6) == 0)
   {
      colour.r = 92;
      colour.g = 92;
      colour.b = 92;
   }
   else if (strncasecmp(x11ColourName,"grey36",6) == 0)
   {
      colour.r = 92;
      colour.g = 92;
      colour.b = 92;
   }
   else if (strncasecmp(x11ColourName,"gray37",6) == 0)
   {
      colour.r = 94;
      colour.g = 94;
      colour.b = 94;
   }
   else if (strncasecmp(x11ColourName,"grey37",6) == 0)
   {
      colour.r = 94;
      colour.g = 94;
      colour.b = 94;
   }
   else if (strncasecmp(x11ColourName,"gray38",6) == 0)
   {
      colour.r = 97;
      colour.g = 97;
      colour.b = 97;
   }
   else if (strncasecmp(x11ColourName,"grey38",6) == 0)
   {
      colour.r = 97;
      colour.g = 97;
      colour.b = 97;
   }
   else if (strncasecmp(x11ColourName,"gray39",6) == 0)
   {
      colour.r = 99;
      colour.g = 99;
      colour.b = 99;
   }
   else if (strncasecmp(x11ColourName,"grey39",6) == 0)
   {
      colour.r = 99;
      colour.g = 99;
      colour.b = 99;
   }
   else if (strncasecmp(x11ColourName,"gray40",6) == 0)
   {
      colour.r = 102;
      colour.g = 102;
      colour.b = 102;
   }
   else if (strncasecmp(x11ColourName,"grey40",6) == 0)
   {
      colour.r = 102;
      colour.g = 102;
      colour.b = 102;
   }
   else if (strncasecmp(x11ColourName,"gray41",6) == 0)
   {
      colour.r = 105;
      colour.g = 105;
      colour.b = 105;
   }
   else if (strncasecmp(x11ColourName,"grey41",6) == 0)
   {
      colour.r = 105;
      colour.g = 105;
      colour.b = 105;
   }
   else if (strncasecmp(x11ColourName,"gray42",6) == 0)
   {
      colour.r = 107;
      colour.g = 107;
      colour.b = 107;
   }
   else if (strncasecmp(x11ColourName,"grey42",6) == 0)
   {
      colour.r = 107;
      colour.g = 107;
      colour.b = 107;
   }
   else if (strncasecmp(x11ColourName,"gray43",6) == 0)
   {
      colour.r = 110;
      colour.g = 110;
      colour.b = 110;
   }
   else if (strncasecmp(x11ColourName,"grey43",6) == 0)
   {
      colour.r = 110;
      colour.g = 110;
      colour.b = 110;
   }
   else if (strncasecmp(x11ColourName,"gray44",6) == 0)
   {
      colour.r = 112;
      colour.g = 112;
      colour.b = 112;
   }
   else if (strncasecmp(x11ColourName,"grey44",6) == 0)
   {
      colour.r = 112;
      colour.g = 112;
      colour.b = 112;
   }
   else if (strncasecmp(x11ColourName,"gray45",6) == 0)
   {
      colour.r = 115;
      colour.g = 115;
      colour.b = 115;
   }
   else if (strncasecmp(x11ColourName,"grey45",6) == 0)
   {
      colour.r = 115;
      colour.g = 115;
      colour.b = 115;
   }
   else if (strncasecmp(x11ColourName,"gray46",6) == 0)
   {
      colour.r = 117;
      colour.g = 117;
      colour.b = 117;
   }
   else if (strncasecmp(x11ColourName,"grey46",6) == 0)
   {
      colour.r = 117;
      colour.g = 117;
      colour.b = 117;
   }
   else if (strncasecmp(x11ColourName,"gray47",6) == 0)
   {
      colour.r = 120;
      colour.g = 120;
      colour.b = 120;
   }
   else if (strncasecmp(x11ColourName,"grey47",6) == 0)
   {
      colour.r = 120;
      colour.g = 120;
      colour.b = 120;
   }
   else if (strncasecmp(x11ColourName,"gray48",6) == 0)
   {
      colour.r = 122;
      colour.g = 122;
      colour.b = 122;
   }
   else if (strncasecmp(x11ColourName,"grey48",6) == 0)
   {
      colour.r = 122;
      colour.g = 122;
      colour.b = 122;
   }
   else if (strncasecmp(x11ColourName,"gray49",6) == 0)
   {
      colour.r = 125;
      colour.g = 125;
      colour.b = 125;
   }
   else if (strncasecmp(x11ColourName,"grey49",6) == 0)
   {
      colour.r = 125;
      colour.g = 125;
      colour.b = 125;
   }
   else if (strncasecmp(x11ColourName,"gray50",6) == 0)
   {
      colour.r = 127;
      colour.g = 127;
      colour.b = 127;
   }
   else if (strncasecmp(x11ColourName,"grey50",6) == 0)
   {
      colour.r = 127;
      colour.g = 127;
      colour.b = 127;
   }
   else if (strncasecmp(x11ColourName,"gray51",6) == 0)
   {
      colour.r = 130;
      colour.g = 130;
      colour.b = 130;
   }
   else if (strncasecmp(x11ColourName,"grey51",6) == 0)
   {
      colour.r = 130;
      colour.g = 130;
      colour.b = 130;
   }
   else if (strncasecmp(x11ColourName,"gray52",6) == 0)
   {
      colour.r = 133;
      colour.g = 133;
      colour.b = 133;
   }
   else if (strncasecmp(x11ColourName,"grey52",6) == 0)
   {
      colour.r = 133;
      colour.g = 133;
      colour.b = 133;
   }
   else if (strncasecmp(x11ColourName,"gray53",6) == 0)
   {
      colour.r = 135;
      colour.g = 135;
      colour.b = 135;
   }
   else if (strncasecmp(x11ColourName,"grey53",6) == 0)
   {
      colour.r = 135;
      colour.g = 135;
      colour.b = 135;
   }
   else if (strncasecmp(x11ColourName,"gray54",6) == 0)
   {
      colour.r = 138;
      colour.g = 138;
      colour.b = 138;
   }
   else if (strncasecmp(x11ColourName,"grey54",6) == 0)
   {
      colour.r = 138;
      colour.g = 138;
      colour.b = 138;
   }
   else if (strncasecmp(x11ColourName,"gray55",6) == 0)
   {
      colour.r = 140;
      colour.g = 140;
      colour.b = 140;
   }
   else if (strncasecmp(x11ColourName,"grey55",6) == 0)
   {
      colour.r = 140;
      colour.g = 140;
      colour.b = 140;
   }
   else if (strncasecmp(x11ColourName,"gray56",6) == 0)
   {
      colour.r = 143;
      colour.g = 143;
      colour.b = 143;
   }
   else if (strncasecmp(x11ColourName,"grey56",6) == 0)
   {
      colour.r = 143;
      colour.g = 143;
      colour.b = 143;
   }
   else if (strncasecmp(x11ColourName,"gray57",6) == 0)
   {
      colour.r = 145;
      colour.g = 145;
      colour.b = 145;
   }
   else if (strncasecmp(x11ColourName,"grey57",6) == 0)
   {
      colour.r = 145;
      colour.g = 145;
      colour.b = 145;
   }
   else if (strncasecmp(x11ColourName,"gray58",6) == 0)
   {
      colour.r = 148;
      colour.g = 148;
      colour.b = 148;
   }
   else if (strncasecmp(x11ColourName,"grey58",6) == 0)
   {
      colour.r = 148;
      colour.g = 148;
      colour.b = 148;
   }
   else if (strncasecmp(x11ColourName,"gray59",6) == 0)
   {
      colour.r = 150;
      colour.g = 150;
      colour.b = 150;
   }
   else if (strncasecmp(x11ColourName,"grey59",6) == 0)
   {
      colour.r = 150;
      colour.g = 150;
      colour.b = 150;
   }
   else if (strncasecmp(x11ColourName,"gray60",6) == 0)
   {
      colour.r = 153;
      colour.g = 153;
      colour.b = 153;
   }
   else if (strncasecmp(x11ColourName,"grey60",6) == 0)
   {
      colour.r = 153;
      colour.g = 153;
      colour.b = 153;
   }
   else if (strncasecmp(x11ColourName,"gray61",6) == 0)
   {
      colour.r = 156;
      colour.g = 156;
      colour.b = 156;
   }
   else if (strncasecmp(x11ColourName,"grey61",6) == 0)
   {
      colour.r = 156;
      colour.g = 156;
      colour.b = 156;
   }
   else if (strncasecmp(x11ColourName,"gray62",6) == 0)
   {
      colour.r = 158;
      colour.g = 158;
      colour.b = 158;
   }
   else if (strncasecmp(x11ColourName,"grey62",6) == 0)
   {
      colour.r = 158;
      colour.g = 158;
      colour.b = 158;
   }
   else if (strncasecmp(x11ColourName,"gray63",6) == 0)
   {
      colour.r = 161;
      colour.g = 161;
      colour.b = 161;
   }
   else if (strncasecmp(x11ColourName,"grey63",6) == 0)
   {
      colour.r = 161;
      colour.g = 161;
      colour.b = 161;
   }
   else if (strncasecmp(x11ColourName,"gray64",6) == 0)
   {
      colour.r = 163;
      colour.g = 163;
      colour.b = 163;
   }
   else if (strncasecmp(x11ColourName,"grey64",6) == 0)
   {
      colour.r = 163;
      colour.g = 163;
      colour.b = 163;
   }
   else if (strncasecmp(x11ColourName,"gray65",6) == 0)
   {
      colour.r = 166;
      colour.g = 166;
      colour.b = 166;
   }
   else if (strncasecmp(x11ColourName,"grey65",6) == 0)
   {
      colour.r = 166;
      colour.g = 166;
      colour.b = 166;
   }
   else if (strncasecmp(x11ColourName,"gray66",6) == 0)
   {
      colour.r = 168;
      colour.g = 168;
      colour.b = 168;
   }
   else if (strncasecmp(x11ColourName,"grey66",6) == 0)
   {
      colour.r = 168;
      colour.g = 168;
      colour.b = 168;
   }
   else if (strncasecmp(x11ColourName,"gray67",6) == 0)
   {
      colour.r = 171;
      colour.g = 171;
      colour.b = 171;
   }
   else if (strncasecmp(x11ColourName,"grey67",6) == 0)
   {
      colour.r = 171;
      colour.g = 171;
      colour.b = 171;
   }
   else if (strncasecmp(x11ColourName,"gray68",6) == 0)
   {
      colour.r = 173;
      colour.g = 173;
      colour.b = 173;
   }
   else if (strncasecmp(x11ColourName,"grey68",6) == 0)
   {
      colour.r = 173;
      colour.g = 173;
      colour.b = 173;
   }
   else if (strncasecmp(x11ColourName,"gray69",6) == 0)
   {
      colour.r = 176;
      colour.g = 176;
      colour.b = 176;
   }
   else if (strncasecmp(x11ColourName,"grey69",6) == 0)
   {
      colour.r = 176;
      colour.g = 176;
      colour.b = 176;
   }
   else if (strncasecmp(x11ColourName,"gray70",6) == 0)
   {
      colour.r = 179;
      colour.g = 179;
      colour.b = 179;
   }
   else if (strncasecmp(x11ColourName,"grey70",6) == 0)
   {
      colour.r = 179;
      colour.g = 179;
      colour.b = 179;
   }
   else if (strncasecmp(x11ColourName,"gray71",6) == 0)
   {
      colour.r = 181;
      colour.g = 181;
      colour.b = 181;
   }
   else if (strncasecmp(x11ColourName,"grey71",6) == 0)
   {
      colour.r = 181;
      colour.g = 181;
      colour.b = 181;
   }
   else if (strncasecmp(x11ColourName,"gray72",6) == 0)
   {
      colour.r = 184;
      colour.g = 184;
      colour.b = 184;
   }
   else if (strncasecmp(x11ColourName,"grey72",6) == 0)
   {
      colour.r = 184;
      colour.g = 184;
      colour.b = 184;
   }
   else if (strncasecmp(x11ColourName,"gray73",6) == 0)
   {
      colour.r = 186;
      colour.g = 186;
      colour.b = 186;
   }
   else if (strncasecmp(x11ColourName,"grey73",6) == 0)
   {
      colour.r = 186;
      colour.g = 186;
      colour.b = 186;
   }
   else if (strncasecmp(x11ColourName,"gray74",6) == 0)
   {
      colour.r = 189;
      colour.g = 189;
      colour.b = 189;
   }
   else if (strncasecmp(x11ColourName,"grey74",6) == 0)
   {
      colour.r = 189;
      colour.g = 189;
      colour.b = 189;
   }
   else if (strncasecmp(x11ColourName,"gray75",6) == 0)
   {
      colour.r = 191;
      colour.g = 191;
      colour.b = 191;
   }
   else if (strncasecmp(x11ColourName,"grey75",6) == 0)
   {
      colour.r = 191;
      colour.g = 191;
      colour.b = 191;
   }
   else if (strncasecmp(x11ColourName,"gray76",6) == 0)
   {
      colour.r = 194;
      colour.g = 194;
      colour.b = 194;
   }
   else if (strncasecmp(x11ColourName,"grey76",6) == 0)
   {
      colour.r = 194;
      colour.g = 194;
      colour.b = 194;
   }
   else if (strncasecmp(x11ColourName,"gray77",6) == 0)
   {
      colour.r = 196;
      colour.g = 196;
      colour.b = 196;
   }
   else if (strncasecmp(x11ColourName,"grey77",6) == 0)
   {
      colour.r = 196;
      colour.g = 196;
      colour.b = 196;
   }
   else if (strncasecmp(x11ColourName,"gray78",6) == 0)
   {
      colour.r = 199;
      colour.g = 199;
      colour.b = 199;
   }
   else if (strncasecmp(x11ColourName,"grey78",6) == 0)
   {
      colour.r = 199;
      colour.g = 199;
      colour.b = 199;
   }
   else if (strncasecmp(x11ColourName,"gray79",6) == 0)
   {
      colour.r = 201;
      colour.g = 201;
      colour.b = 201;
   }
   else if (strncasecmp(x11ColourName,"grey79",6) == 0)
   {
      colour.r = 201;
      colour.g = 201;
      colour.b = 201;
   }
   else if (strncasecmp(x11ColourName,"gray80",6) == 0)
   {
      colour.r = 204;
      colour.g = 204;
      colour.b = 204;
   }
   else if (strncasecmp(x11ColourName,"grey80",6) == 0)
   {
      colour.r = 204;
      colour.g = 204;
      colour.b = 204;
   }
   else if (strncasecmp(x11ColourName,"gray81",6) == 0)
   {
      colour.r = 207;
      colour.g = 207;
      colour.b = 207;
   }
   else if (strncasecmp(x11ColourName,"grey81",6) == 0)
   {
      colour.r = 207;
      colour.g = 207;
      colour.b = 207;
   }
   else if (strncasecmp(x11ColourName,"gray82",6) == 0)
   {
      colour.r = 209;
      colour.g = 209;
      colour.b = 209;
   }
   else if (strncasecmp(x11ColourName,"grey82",6) == 0)
   {
      colour.r = 209;
      colour.g = 209;
      colour.b = 209;
   }
   else if (strncasecmp(x11ColourName,"gray83",6) == 0)
   {
      colour.r = 212;
      colour.g = 212;
      colour.b = 212;
   }
   else if (strncasecmp(x11ColourName,"grey83",6) == 0)
   {
      colour.r = 212;
      colour.g = 212;
      colour.b = 212;
   }
   else if (strncasecmp(x11ColourName,"gray84",6) == 0)
   {
      colour.r = 214;
      colour.g = 214;
      colour.b = 214;
   }
   else if (strncasecmp(x11ColourName,"grey84",6) == 0)
   {
      colour.r = 214;
      colour.g = 214;
      colour.b = 214;
   }
   else if (strncasecmp(x11ColourName,"gray85",6) == 0)
   {
      colour.r = 217;
      colour.g = 217;
      colour.b = 217;
   }
   else if (strncasecmp(x11ColourName,"grey85",6) == 0)
   {
      colour.r = 217;
      colour.g = 217;
      colour.b = 217;
   }
   else if (strncasecmp(x11ColourName,"gray86",6) == 0)
   {
      colour.r = 219;
      colour.g = 219;
      colour.b = 219;
   }
   else if (strncasecmp(x11ColourName,"grey86",6) == 0)
   {
      colour.r = 219;
      colour.g = 219;
      colour.b = 219;
   }
   else if (strncasecmp(x11ColourName,"gray87",6) == 0)
   {
      colour.r = 222;
      colour.g = 222;
      colour.b = 222;
   }
   else if (strncasecmp(x11ColourName,"grey87",6) == 0)
   {
      colour.r = 222;
      colour.g = 222;
      colour.b = 222;
   }
   else if (strncasecmp(x11ColourName,"gray88",6) == 0)
   {
      colour.r = 224;
      colour.g = 224;
      colour.b = 224;
   }
   else if (strncasecmp(x11ColourName,"grey88",6) == 0)
   {
      colour.r = 224;
      colour.g = 224;
      colour.b = 224;
   }
   else if (strncasecmp(x11ColourName,"gray89",6) == 0)
   {
      colour.r = 227;
      colour.g = 227;
      colour.b = 227;
   }
   else if (strncasecmp(x11ColourName,"grey89",6) == 0)
   {
      colour.r = 227;
      colour.g = 227;
      colour.b = 227;
   }
   else if (strncasecmp(x11ColourName,"gray90",6) == 0)
   {
      colour.r = 229;
      colour.g = 229;
      colour.b = 229;
   }
   else if (strncasecmp(x11ColourName,"grey90",6) == 0)
   {
      colour.r = 229;
      colour.g = 229;
      colour.b = 229;
   }
   else if (strncasecmp(x11ColourName,"gray91",6) == 0)
   {
      colour.r = 232;
      colour.g = 232;
      colour.b = 232;
   }
   else if (strncasecmp(x11ColourName,"grey91",6) == 0)
   {
      colour.r = 232;
      colour.g = 232;
      colour.b = 232;
   }
   else if (strncasecmp(x11ColourName,"gray92",6) == 0)
   {
      colour.r = 235;
      colour.g = 235;
      colour.b = 235;
   }
   else if (strncasecmp(x11ColourName,"grey92",6) == 0)
   {
      colour.r = 235;
      colour.g = 235;
      colour.b = 235;
   }
   else if (strncasecmp(x11ColourName,"gray93",6) == 0)
   {
      colour.r = 237;
      colour.g = 237;
      colour.b = 237;
   }
   else if (strncasecmp(x11ColourName,"grey93",6) == 0)
   {
      colour.r = 237;
      colour.g = 237;
      colour.b = 237;
   }
   else if (strncasecmp(x11ColourName,"gray94",6) == 0)
   {
      colour.r = 240;
      colour.g = 240;
      colour.b = 240;
   }
   else if (strncasecmp(x11ColourName,"grey94",6) == 0)
   {
      colour.r = 240;
      colour.g = 240;
      colour.b = 240;
   }
   else if (strncasecmp(x11ColourName,"gray95",6) == 0)
   {
      colour.r = 242;
      colour.g = 242;
      colour.b = 242;
   }
   else if (strncasecmp(x11ColourName,"grey95",6) == 0)
   {
      colour.r = 242;
      colour.g = 242;
      colour.b = 242;
   }
   else if (strncasecmp(x11ColourName,"gray96",6) == 0)
   {
      colour.r = 245;
      colour.g = 245;
      colour.b = 245;
   }
   else if (strncasecmp(x11ColourName,"grey96",6) == 0)
   {
      colour.r = 245;
      colour.g = 245;
      colour.b = 245;
   }
   else if (strncasecmp(x11ColourName,"gray97",6) == 0)
   {
      colour.r = 247;
      colour.g = 247;
      colour.b = 247;
   }
   else if (strncasecmp(x11ColourName,"grey97",6) == 0)
   {
      colour.r = 247;
      colour.g = 247;
      colour.b = 247;
   }
   else if (strncasecmp(x11ColourName,"gray98",6) == 0)
   {
      colour.r = 250;
      colour.g = 250;
      colour.b = 250;
   }
   else if (strncasecmp(x11ColourName,"grey98",6) == 0)
   {
      colour.r = 250;
      colour.g = 250;
      colour.b = 250;
   }
   else if (strncasecmp(x11ColourName,"gray99",6) == 0)
   {
      colour.r = 252;
      colour.g = 252;
      colour.b = 252;
   }
   else if (strncasecmp(x11ColourName,"grey99",6) == 0)
   {
      colour.r = 252;
      colour.g = 252;
      colour.b = 252;
   }
   else if (strncasecmp(x11ColourName,"gray100",7) == 0)
   {
      colour.r = 255;
      colour.g = 255;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"grey100",7) == 0)
   {
      colour.r = 255;
      colour.g = 255;
      colour.b = 255;
   }
   else if (strncasecmp(x11ColourName,"DarkGrey",8) == 0)
   {
      colour.r = 169;
      colour.g = 169;
      colour.b = 169;
   }
   else if (strncasecmp(x11ColourName,"DarkGray",8) == 0)
   {
      colour.r = 169;
      colour.g = 169;
      colour.b = 169;
   }
   else if (strncasecmp(x11ColourName,"DarkBlue",8) == 0)
   {
      colour.r = 0;
      colour.g = 0;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"DarkCyan",8) == 0)
   {
      colour.r = 0;
      colour.g = 139;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"DarkMagenta",11) == 0)
   {
      colour.r = 139;
      colour.g = 0;
      colour.b = 139;
   }
   else if (strncasecmp(x11ColourName,"DarkRed",7) == 0)
   {
      colour.r = 139;
      colour.g = 0;
      colour.b = 0;
   }
   else if (strncasecmp(x11ColourName,"LightGreen",10) == 0)
   {
      colour.r = 144;
      colour.g = 238;
      colour.b = 144;
   }
   else        /* Default is Black */
   {
      colour.r = 0;
      colour.g = 0;
      colour.b = 0;
   }

   return colour;
}

Colour Colour_FromHex(std::string hexName)
{
   Colour colour;
   int i;

   /* Check to make sure colour is valid */
   if (hexName.at(0) != '#')
   {
      debug_print( "Cannot recognise hex colour %s.\n", hexName.c_str());
      return colour;
   }
   for (i = 1 ; i <= 6 ; i++)
   {
      if (isxdigit(hexName.at(i)) == 0)
      {
         printf( "Cannot recognise hex colour %s.\n", hexName.c_str());
         return colour;
      }
   }

   /* Seperate colours */
   std::string red = hexName.substr(0, 2);
   std::string grn = hexName.substr(2, 2);
   std::string blu = hexName.substr(4, 2);

   /* Read colours */
   sscanf(red.c_str(), "%x", &colour.r);
   sscanf(grn.c_str(), "%x", &colour.g);
   sscanf(blu.c_str(), "%x", &colour.b);

   return colour;
}


