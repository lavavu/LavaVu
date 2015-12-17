#include "GraphicsUtil.h"
#include "Colours.h"

Colour Colour_RGBA(int r, int g, int b, int a)
{
  Colour colour;
  colour.r = r;
  colour.g = g;
  colour.b = b;
  colour.a = a;
  return colour;
}

std::ostream & operator<<(std::ostream &os, const Colour& colour)
{
  return os << "rgba(" << (int)colour.r << "," << (int)colour.g << "," << (int)colour.b << "," << (int)colour.a << ")";
}

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

  /* Get Opacity From String */
  /* Opacity must be read in after the ":" of the name of the colour */
  int pos = str.find(":");
  if (pos != std::string::npos)
  {
    std::stringstream ss(str.substr(pos+1));
    if (!(ss >> opacity))
      opacity = 1.0;
    str = str.substr(0, pos);
  }

  Colour colour = Colour_FromX11Colour(str);

  colour.a = opacity * 255;
  return colour;
}

/* Reads hex or colour from X11 Colour Chart */
/* Defaults to black if anything else */

Colour Colour_FromX11Colour(std::string x11colour)
{
  std::transform(x11colour.begin(), x11colour.end(), x11colour.begin(), ::tolower);
  if (x11colour.at(0) == '#')
    return Colour_FromHex(x11colour);
  if (x11colour == "snow")
    return Colour_RGBA(255, 250, 250);
  if (x11colour == "ghostwhite")
    return Colour_RGBA(248, 248, 255);
  if (x11colour == "whitesmoke")
    return Colour_RGBA(245, 245, 245);
  if (x11colour == "gainsboro")
    return Colour_RGBA(220, 220, 220);
  if (x11colour == "floralwhite")
    return Colour_RGBA(255, 250, 240);
  if (x11colour == "oldlace")
    return Colour_RGBA(253, 245, 230);
  if (x11colour == "linen")
    return Colour_RGBA(250, 240, 230);
  if (x11colour == "antiquewhite")
    return Colour_RGBA(250, 235, 215);
  if (x11colour == "papayawhip")
    return Colour_RGBA(255, 239, 213);
  if (x11colour == "blanchedalmond")
    return Colour_RGBA(255, 235, 205);
  if (x11colour == "bisque")
    return Colour_RGBA(255, 228, 196);
  if (x11colour == "peachpuff")
    return Colour_RGBA(255, 218, 185);
  if (x11colour == "navajowhite")
    return Colour_RGBA(255, 222, 173);
  if (x11colour == "moccasin")
    return Colour_RGBA(255, 228, 181);
  if (x11colour == "cornsilk")
    return Colour_RGBA(255, 248, 220);
  if (x11colour == "ivory")
    return Colour_RGBA(255, 255, 240);
  if (x11colour == "lemonchiffon")
    return Colour_RGBA(255, 250, 205);
  if (x11colour == "seashell")
    return Colour_RGBA(255, 245, 238);
  if (x11colour == "honeydew")
    return Colour_RGBA(240, 255, 240);
  if (x11colour == "mintcream")
    return Colour_RGBA(245, 255, 250);
  if (x11colour == "azure")
    return Colour_RGBA(240, 255, 255);
  if (x11colour == "aliceblue")
    return Colour_RGBA(240, 248, 255);
  if (x11colour == "lavender")
    return Colour_RGBA(230, 230, 250);
  if (x11colour == "lavenderblush")
    return Colour_RGBA(255, 240, 245);
  if (x11colour == "mistyrose")
    return Colour_RGBA(255, 228, 225);
  if (x11colour == "white")
    return Colour_RGBA(255, 255, 255);
  if (x11colour == "black")
    return Colour_RGBA(0, 0, 0);
  if (x11colour == "darkslategray")
    return Colour_RGBA(47, 79, 79);
  if (x11colour == "darkslategrey")
    return Colour_RGBA(47, 79, 79);
  if (x11colour == "dimgray")
    return Colour_RGBA(105, 105, 105);
  if (x11colour == "dimgrey")
    return Colour_RGBA(105, 105, 105);
  if (x11colour == "slategray")
    return Colour_RGBA(112, 128, 144);
  if (x11colour == "slategrey")
    return Colour_RGBA(112, 128, 144);
  if (x11colour == "lightslategray")
    return Colour_RGBA(119, 136, 153);
  if (x11colour == "lightslategrey")
    return Colour_RGBA(119, 136, 153);
  if (x11colour == "gray")
    return Colour_RGBA(190, 190, 190);
  if (x11colour == "grey")
    return Colour_RGBA(190, 190, 190);
  if (x11colour == "lightgrey")
    return Colour_RGBA(211, 211, 211);
  if (x11colour == "lightgray")
    return Colour_RGBA(211, 211, 211);
  if (x11colour == "midnightblue")
    return Colour_RGBA(25, 25, 112);
  if (x11colour == "navy")
    return Colour_RGBA(0, 0, 128);
  if (x11colour == "navyblue")
    return Colour_RGBA(0, 0, 128);
  if (x11colour == "cornflowerblue")
    return Colour_RGBA(100, 149, 237);
  if (x11colour == "darkslateblue")
    return Colour_RGBA(72, 61, 139);
  if (x11colour == "slateblue")
    return Colour_RGBA(106, 90, 205);
  if (x11colour == "mediumslateblue")
    return Colour_RGBA(123, 104, 238);
  if (x11colour == "lightslateblue")
    return Colour_RGBA(132, 112, 255);
  if (x11colour == "mediumblue")
    return Colour_RGBA(0, 0, 205);
  if (x11colour == "royalblue")
    return Colour_RGBA(65, 105, 225);
  if (x11colour == "blue")
    return Colour_RGBA(0, 0, 255);
  if (x11colour == "dodgerblue")
    return Colour_RGBA(30, 144, 255);
  if (x11colour == "deepskyblue")
    return Colour_RGBA(0, 191, 255);
  if (x11colour == "skyblue")
    return Colour_RGBA(135, 206, 235);
  if (x11colour == "lightskyblue")
    return Colour_RGBA(135, 206, 250);
  if (x11colour == "steelblue")
    return Colour_RGBA(70, 130, 180);
  if (x11colour == "lightsteelblue")
    return Colour_RGBA(176, 196, 222);
  if (x11colour == "lightblue")
    return Colour_RGBA(173, 216, 230);
  if (x11colour == "powderblue")
    return Colour_RGBA(176, 224, 230);
  if (x11colour == "paleturquoise")
    return Colour_RGBA(175, 238, 238);
  if (x11colour == "darkturquoise")
    return Colour_RGBA(0, 206, 209);
  if (x11colour == "mediumturquoise")
    return Colour_RGBA(72, 209, 204);
  if (x11colour == "turquoise")
    return Colour_RGBA(64, 224, 208);
  if (x11colour == "cyan")
    return Colour_RGBA(0, 255, 255);
  if (x11colour == "lightcyan")
    return Colour_RGBA(224, 255, 255);
  if (x11colour == "cadetblue")
    return Colour_RGBA(95, 158, 160);
  if (x11colour == "mediumaquamarine")
    return Colour_RGBA(102, 205, 170);
  if (x11colour == "aquamarine")
    return Colour_RGBA(127, 255, 212);
  if (x11colour == "darkgreen")
    return Colour_RGBA(0, 100, 0);
  if (x11colour == "darkolivegreen")
    return Colour_RGBA(85, 107, 47);
  if (x11colour == "darkseagreen")
    return Colour_RGBA(143, 188, 143);
  if (x11colour == "seagreen")
    return Colour_RGBA(46, 139, 87);
  if (x11colour == "mediumseagreen")
    return Colour_RGBA(60, 179, 113);
  if (x11colour == "lightseagreen")
    return Colour_RGBA(32, 178, 170);
  if (x11colour == "palegreen")
    return Colour_RGBA(152, 251, 152);
  if (x11colour == "springgreen")
    return Colour_RGBA(0, 255, 127);
  if (x11colour == "lawngreen")
    return Colour_RGBA(124, 252, 0);
  if (x11colour == "green")
    return Colour_RGBA(0, 255, 0);
  if (x11colour == "chartreuse")
    return Colour_RGBA(127, 255, 0);
  if (x11colour == "mediumspringgreen")
    return Colour_RGBA(0, 250, 154);
  if (x11colour == "greenyellow")
    return Colour_RGBA(173, 255, 47);
  if (x11colour == "limegreen")
    return Colour_RGBA(50, 205, 50);
  if (x11colour == "yellowgreen")
    return Colour_RGBA(154, 205, 50);
  if (x11colour == "forestgreen")
    return Colour_RGBA(34, 139, 34);
  if (x11colour == "olivedrab")
    return Colour_RGBA(107, 142, 35);
  if (x11colour == "darkkhaki")
    return Colour_RGBA(189, 183, 107);
  if (x11colour == "khaki")
    return Colour_RGBA(240, 230, 140);
  if (x11colour == "palegoldenrod")
    return Colour_RGBA(238, 232, 170);
  if (x11colour == "lightgoldenrodyellow")
    return Colour_RGBA(250, 250, 210);
  if (x11colour == "lightyellow")
    return Colour_RGBA(255, 255, 224);
  if (x11colour == "yellow")
    return Colour_RGBA(255, 255, 0);
  if (x11colour == "gold")
    return Colour_RGBA(255, 215, 0);
  if (x11colour == "lightgoldenrod")
    return Colour_RGBA(238, 221, 130);
  if (x11colour == "goldenrod")
    return Colour_RGBA(218, 165, 32);
  if (x11colour == "darkgoldenrod")
    return Colour_RGBA(184, 134, 11);
  if (x11colour == "rosybrown")
    return Colour_RGBA(188, 143, 143);
  if (x11colour == "indianred")
    return Colour_RGBA(205, 92, 92);
  if (x11colour == "saddlebrown")
    return Colour_RGBA(139, 69, 19);
  if (x11colour == "sienna")
    return Colour_RGBA(160, 82, 45);
  if (x11colour == "peru")
    return Colour_RGBA(205, 133, 63);
  if (x11colour == "burlywood")
    return Colour_RGBA(222, 184, 135);
  if (x11colour == "beige")
    return Colour_RGBA(245, 245, 220);
  if (x11colour == "wheat")
    return Colour_RGBA(245, 222, 179);
  if (x11colour == "sandybrown")
    return Colour_RGBA(244, 164, 96);
  if (x11colour == "tan")
    return Colour_RGBA(210, 180, 140);
  if (x11colour == "chocolate")
    return Colour_RGBA(210, 105, 30);
  if (x11colour == "firebrick")
    return Colour_RGBA(178, 34, 34);
  if (x11colour == "brown")
    return Colour_RGBA(165, 42, 42);
  if (x11colour == "darksalmon")
    return Colour_RGBA(233, 150, 122);
  if (x11colour == "salmon")
    return Colour_RGBA(250, 128, 114);
  if (x11colour == "lightsalmon")
    return Colour_RGBA(255, 160, 122);
  if (x11colour == "orange")
    return Colour_RGBA(255, 165, 0);
  if (x11colour == "darkorange")
    return Colour_RGBA(255, 140, 0);
  if (x11colour == "coral")
    return Colour_RGBA(255, 127, 80);
  if (x11colour == "lightcoral")
    return Colour_RGBA(240, 128, 128);
  if (x11colour == "tomato")
    return Colour_RGBA(255, 99, 71);
  if (x11colour == "orangered")
    return Colour_RGBA(255, 69, 0);
  if (x11colour == "red")
    return Colour_RGBA(255, 0, 0);
  if (x11colour == "hotpink")
    return Colour_RGBA(255, 105, 180);
  if (x11colour == "deeppink")
    return Colour_RGBA(255, 20, 147);
  if (x11colour == "pink")
    return Colour_RGBA(255, 192, 203);
  if (x11colour == "lightpink")
    return Colour_RGBA(255, 182, 193);
  if (x11colour == "palevioletred")
    return Colour_RGBA(219, 112, 147);
  if (x11colour == "maroon")
    return Colour_RGBA(176, 48, 96);
  if (x11colour == "mediumvioletred")
    return Colour_RGBA(199, 21, 133);
  if (x11colour == "violetred")
    return Colour_RGBA(208, 32, 144);
  if (x11colour == "magenta")
    return Colour_RGBA(255, 0, 255);
  if (x11colour == "violet")
    return Colour_RGBA(238, 130, 238);
  if (x11colour == "plum")
    return Colour_RGBA(221, 160, 221);
  if (x11colour == "orchid")
    return Colour_RGBA(218, 112, 214);
  if (x11colour == "mediumorchid")
    return Colour_RGBA(186, 85, 211);
  if (x11colour == "darkorchid")
    return Colour_RGBA(153, 50, 204);
  if (x11colour == "darkviolet")
    return Colour_RGBA(148, 0, 211);
  if (x11colour == "blueviolet")
    return Colour_RGBA(138, 43, 226);
  if (x11colour == "purple")
    return Colour_RGBA(160, 32, 240);
  if (x11colour == "mediumpurple")
    return Colour_RGBA(147, 112, 219);
  if (x11colour == "thistle")
    return Colour_RGBA(216, 191, 216);
  if (x11colour == "snow1")
    return Colour_RGBA(255, 250, 250);
  if (x11colour == "snow2")
    return Colour_RGBA(238, 233, 233);
  if (x11colour == "snow3")
    return Colour_RGBA(205, 201, 201);
  if (x11colour == "snow4")
    return Colour_RGBA(139, 137, 137);
  if (x11colour == "seashell1")
    return Colour_RGBA(255, 245, 238);
  if (x11colour == "seashell2")
    return Colour_RGBA(238, 229, 222);
  if (x11colour == "seashell3")
    return Colour_RGBA(205, 197, 191);
  if (x11colour == "seashell4")
    return Colour_RGBA(139, 134, 130);
  if (x11colour == "antiquewhite1")
    return Colour_RGBA(255, 239, 219);
  if (x11colour == "antiquewhite2")
    return Colour_RGBA(238, 223, 204);
  if (x11colour == "antiquewhite3")
    return Colour_RGBA(205, 192, 176);
  if (x11colour == "antiquewhite4")
    return Colour_RGBA(139, 131, 120);
  if (x11colour == "bisque1")
    return Colour_RGBA(255, 228, 196);
  if (x11colour == "bisque2")
    return Colour_RGBA(238, 213, 183);
  if (x11colour == "bisque3")
    return Colour_RGBA(205, 183, 158);
  if (x11colour == "bisque4")
    return Colour_RGBA(139, 125, 107);
  if (x11colour == "peachpuff1")
    return Colour_RGBA(255, 218, 185);
  if (x11colour == "peachpuff2")
    return Colour_RGBA(238, 203, 173);
  if (x11colour == "peachpuff3")
    return Colour_RGBA(205, 175, 149);
  if (x11colour == "peachpuff4")
    return Colour_RGBA(139, 119, 101);
  if (x11colour == "navajowhite1")
    return Colour_RGBA(255, 222, 173);
  if (x11colour == "navajowhite2")
    return Colour_RGBA(238, 207, 161);
  if (x11colour == "navajowhite3")
    return Colour_RGBA(205, 179, 139);
  if (x11colour == "navajowhite4")
    return Colour_RGBA(139, 121, 94);
  if (x11colour == "lemonchiffon1")
    return Colour_RGBA(255, 250, 205);
  if (x11colour == "lemonchiffon2")
    return Colour_RGBA(238, 233, 191);
  if (x11colour == "lemonchiffon3")
    return Colour_RGBA(205, 201, 165);
  if (x11colour == "lemonchiffon4")
    return Colour_RGBA(139, 137, 112);
  if (x11colour == "cornsilk1")
    return Colour_RGBA(255, 248, 220);
  if (x11colour == "cornsilk2")
    return Colour_RGBA(238, 232, 205);
  if (x11colour == "cornsilk3")
    return Colour_RGBA(205, 200, 177);
  if (x11colour == "cornsilk4")
    return Colour_RGBA(139, 136, 120);
  if (x11colour == "ivory1")
    return Colour_RGBA(255, 255, 240);
  if (x11colour == "ivory2")
    return Colour_RGBA(238, 238, 224);
  if (x11colour == "ivory3")
    return Colour_RGBA(205, 205, 193);
  if (x11colour == "ivory4")
    return Colour_RGBA(139, 139, 131);
  if (x11colour == "honeydew1")
    return Colour_RGBA(240, 255, 240);
  if (x11colour == "honeydew2")
    return Colour_RGBA(224, 238, 224);
  if (x11colour == "honeydew3")
    return Colour_RGBA(193, 205, 193);
  if (x11colour == "honeydew4")
    return Colour_RGBA(131, 139, 131);
  if (x11colour == "lavenderblush1")
    return Colour_RGBA(255, 240, 245);
  if (x11colour == "lavenderblush2")
    return Colour_RGBA(238, 224, 229);
  if (x11colour == "lavenderblush3")
    return Colour_RGBA(205, 193, 197);
  if (x11colour == "lavenderblush4")
    return Colour_RGBA(139, 131, 134);
  if (x11colour == "mistyrose1")
    return Colour_RGBA(255, 228, 225);
  if (x11colour == "mistyrose2")
    return Colour_RGBA(238, 213, 210);
  if (x11colour == "mistyrose3")
    return Colour_RGBA(205, 183, 181);
  if (x11colour == "mistyrose4")
    return Colour_RGBA(139, 125, 123);
  if (x11colour == "azure1")
    return Colour_RGBA(240, 255, 255);
  if (x11colour == "azure2")
    return Colour_RGBA(224, 238, 238);
  if (x11colour == "azure3")
    return Colour_RGBA(193, 205, 205);
  if (x11colour == "azure4")
    return Colour_RGBA(131, 139, 139);
  if (x11colour == "slateblue1")
    return Colour_RGBA(131, 111, 255);
  if (x11colour == "slateblue2")
    return Colour_RGBA(122, 103, 238);
  if (x11colour == "slateblue3")
    return Colour_RGBA(105, 89, 205);
  if (x11colour == "slateblue4")
    return Colour_RGBA(71, 60, 139);
  if (x11colour == "royalblue1")
    return Colour_RGBA(72, 118, 255);
  if (x11colour == "royalblue2")
    return Colour_RGBA(67, 110, 238);
  if (x11colour == "royalblue3")
    return Colour_RGBA(58, 95, 205);
  if (x11colour == "royalblue4")
    return Colour_RGBA(39, 64, 139);
  if (x11colour == "blue1")
    return Colour_RGBA(0, 0, 255);
  if (x11colour == "blue2")
    return Colour_RGBA(0, 0, 238);
  if (x11colour == "blue3")
    return Colour_RGBA(0, 0, 205);
  if (x11colour == "blue4")
    return Colour_RGBA(0, 0, 139);
  if (x11colour == "dodgerblue1")
    return Colour_RGBA(30, 144, 255);
  if (x11colour == "dodgerblue2")
    return Colour_RGBA(28, 134, 238);
  if (x11colour == "dodgerblue3")
    return Colour_RGBA(24, 116, 205);
  if (x11colour == "dodgerblue4")
    return Colour_RGBA(16, 78, 139);
  if (x11colour == "steelblue1")
    return Colour_RGBA(99, 184, 255);
  if (x11colour == "steelblue2")
    return Colour_RGBA(92, 172, 238);
  if (x11colour == "steelblue3")
    return Colour_RGBA(79, 148, 205);
  if (x11colour == "steelblue4")
    return Colour_RGBA(54, 100, 139);
  if (x11colour == "deepskyblue1")
    return Colour_RGBA(0, 191, 255);
  if (x11colour == "deepskyblue2")
    return Colour_RGBA(0, 178, 238);
  if (x11colour == "deepskyblue3")
    return Colour_RGBA(0, 154, 205);
  if (x11colour == "deepskyblue4")
    return Colour_RGBA(0, 104, 139);
  if (x11colour == "skyblue1")
    return Colour_RGBA(135, 206, 255);
  if (x11colour == "skyblue2")
    return Colour_RGBA(126, 192, 238);
  if (x11colour == "skyblue3")
    return Colour_RGBA(108, 166, 205);
  if (x11colour == "skyblue4")
    return Colour_RGBA(74, 112, 139);
  if (x11colour == "lightskyblue1")
    return Colour_RGBA(176, 226, 255);
  if (x11colour == "lightskyblue2")
    return Colour_RGBA(164, 211, 238);
  if (x11colour == "lightskyblue3")
    return Colour_RGBA(141, 182, 205);
  if (x11colour == "lightskyblue4")
    return Colour_RGBA(96, 123, 139);
  if (x11colour == "slategray1")
    return Colour_RGBA(198, 226, 255);
  if (x11colour == "slategray2")
    return Colour_RGBA(185, 211, 238);
  if (x11colour == "slategray3")
    return Colour_RGBA(159, 182, 205);
  if (x11colour == "slategray4")
    return Colour_RGBA(108, 123, 139);
  if (x11colour == "lightsteelblue1")
    return Colour_RGBA(202, 225, 255);
  if (x11colour == "lightsteelblue2")
    return Colour_RGBA(188, 210, 238);
  if (x11colour == "lightsteelblue3")
    return Colour_RGBA(162, 181, 205);
  if (x11colour == "lightsteelblue4")
    return Colour_RGBA(110, 123, 139);
  if (x11colour == "lightblue1")
    return Colour_RGBA(191, 239, 255);
  if (x11colour == "lightblue2")
    return Colour_RGBA(178, 223, 238);
  if (x11colour == "lightblue3")
    return Colour_RGBA(154, 192, 205);
  if (x11colour == "lightblue4")
    return Colour_RGBA(104, 131, 139);
  if (x11colour == "lightcyan1")
    return Colour_RGBA(224, 255, 255);
  if (x11colour == "lightcyan2")
    return Colour_RGBA(209, 238, 238);
  if (x11colour == "lightcyan3")
    return Colour_RGBA(180, 205, 205);
  if (x11colour == "lightcyan4")
    return Colour_RGBA(122, 139, 139);
  if (x11colour == "paleturquoise1")
    return Colour_RGBA(187, 255, 255);
  if (x11colour == "paleturquoise2")
    return Colour_RGBA(174, 238, 238);
  if (x11colour == "paleturquoise3")
    return Colour_RGBA(150, 205, 205);
  if (x11colour == "paleturquoise4")
    return Colour_RGBA(102, 139, 139);
  if (x11colour == "cadetblue1")
    return Colour_RGBA(152, 245, 255);
  if (x11colour == "cadetblue2")
    return Colour_RGBA(142, 229, 238);
  if (x11colour == "cadetblue3")
    return Colour_RGBA(122, 197, 205);
  if (x11colour == "cadetblue4")
    return Colour_RGBA(83, 134, 139);
  if (x11colour == "turquoise1")
    return Colour_RGBA(0, 245, 255);
  if (x11colour == "turquoise2")
    return Colour_RGBA(0, 229, 238);
  if (x11colour == "turquoise3")
    return Colour_RGBA(0, 197, 205);
  if (x11colour == "turquoise4")
    return Colour_RGBA(0, 134, 139);
  if (x11colour == "cyan1")
    return Colour_RGBA(0, 255, 255);
  if (x11colour == "cyan2")
    return Colour_RGBA(0, 238, 238);
  if (x11colour == "cyan3")
    return Colour_RGBA(0, 205, 205);
  if (x11colour == "cyan4")
    return Colour_RGBA(0, 139, 139);
  if (x11colour == "darkslategray1")
    return Colour_RGBA(151, 255, 255);
  if (x11colour == "darkslategray2")
    return Colour_RGBA(141, 238, 238);
  if (x11colour == "darkslategray3")
    return Colour_RGBA(121, 205, 205);
  if (x11colour == "darkslategray4")
    return Colour_RGBA(82, 139, 139);
  if (x11colour == "aquamarine1")
    return Colour_RGBA(127, 255, 212);
  if (x11colour == "aquamarine2")
    return Colour_RGBA(118, 238, 198);
  if (x11colour == "aquamarine3")
    return Colour_RGBA(102, 205, 170);
  if (x11colour == "aquamarine4")
    return Colour_RGBA(69, 139, 116);
  if (x11colour == "darkseagreen1")
    return Colour_RGBA(193, 255, 193);
  if (x11colour == "darkseagreen2")
    return Colour_RGBA(180, 238, 180);
  if (x11colour == "darkseagreen3")
    return Colour_RGBA(155, 205, 155);
  if (x11colour == "darkseagreen4")
    return Colour_RGBA(105, 139, 105);
  if (x11colour == "seagreen1")
    return Colour_RGBA(84, 255, 159);
  if (x11colour == "seagreen2")
    return Colour_RGBA(78, 238, 148);
  if (x11colour == "seagreen3")
    return Colour_RGBA(67, 205, 128);
  if (x11colour == "seagreen4")
    return Colour_RGBA(46, 139, 87);
  if (x11colour == "palegreen1")
    return Colour_RGBA(154, 255, 154);
  if (x11colour == "palegreen2")
    return Colour_RGBA(144, 238, 144);
  if (x11colour == "palegreen3")
    return Colour_RGBA(124, 205, 124);
  if (x11colour == "palegreen4")
    return Colour_RGBA(84, 139, 84);
  if (x11colour == "springgreen1")
    return Colour_RGBA(0, 255, 127);
  if (x11colour == "springgreen2")
    return Colour_RGBA(0, 238, 118);
  if (x11colour == "springgreen3")
    return Colour_RGBA(0, 205, 102);
  if (x11colour == "springgreen4")
    return Colour_RGBA(0, 139, 69);
  if (x11colour == "green1")
    return Colour_RGBA(0, 255, 0);
  if (x11colour == "green2")
    return Colour_RGBA(0, 238, 0);
  if (x11colour == "green3")
    return Colour_RGBA(0, 205, 0);
  if (x11colour == "green4")
    return Colour_RGBA(0, 139, 0);
  if (x11colour == "chartreuse1")
    return Colour_RGBA(127, 255, 0);
  if (x11colour == "chartreuse2")
    return Colour_RGBA(118, 238, 0);
  if (x11colour == "chartreuse3")
    return Colour_RGBA(102, 205, 0);
  if (x11colour == "chartreuse4")
    return Colour_RGBA(69, 139, 0);
  if (x11colour == "olivedrab1")
    return Colour_RGBA(192, 255, 62);
  if (x11colour == "olivedrab2")
    return Colour_RGBA(179, 238, 58);
  if (x11colour == "olivedrab3")
    return Colour_RGBA(154, 205, 50);
  if (x11colour == "olivedrab4")
    return Colour_RGBA(105, 139, 34);
  if (x11colour == "darkolivegreen1")
    return Colour_RGBA(202, 255, 112);
  if (x11colour == "darkolivegreen2")
    return Colour_RGBA(188, 238, 104);
  if (x11colour == "darkolivegreen3")
    return Colour_RGBA(162, 205, 90);
  if (x11colour == "darkolivegreen4")
    return Colour_RGBA(110, 139, 61);
  if (x11colour == "khaki1")
    return Colour_RGBA(255, 246, 143);
  if (x11colour == "khaki2")
    return Colour_RGBA(238, 230, 133);
  if (x11colour == "khaki3")
    return Colour_RGBA(205, 198, 115);
  if (x11colour == "khaki4")
    return Colour_RGBA(139, 134, 78);
  if (x11colour == "lightgoldenrod1")
    return Colour_RGBA(255, 236, 139);
  if (x11colour == "lightgoldenrod2")
    return Colour_RGBA(238, 220, 130);
  if (x11colour == "lightgoldenrod3")
    return Colour_RGBA(205, 190, 112);
  if (x11colour == "lightgoldenrod4")
    return Colour_RGBA(139, 129, 76);
  if (x11colour == "lightyellow1")
    return Colour_RGBA(255, 255, 224);
  if (x11colour == "lightyellow2")
    return Colour_RGBA(238, 238, 209);
  if (x11colour == "lightyellow3")
    return Colour_RGBA(205, 205, 180);
  if (x11colour == "lightyellow4")
    return Colour_RGBA(139, 139, 122);
  if (x11colour == "yellow1")
    return Colour_RGBA(255, 255, 0);
  if (x11colour == "yellow2")
    return Colour_RGBA(238, 238, 0);
  if (x11colour == "yellow3")
    return Colour_RGBA(205, 205, 0);
  if (x11colour == "yellow4")
    return Colour_RGBA(139, 139, 0);
  if (x11colour == "gold1")
    return Colour_RGBA(255, 215, 0);
  if (x11colour == "gold2")
    return Colour_RGBA(238, 201, 0);
  if (x11colour == "gold3")
    return Colour_RGBA(205, 173, 0);
  if (x11colour == "gold4")
    return Colour_RGBA(139, 117, 0);
  if (x11colour == "goldenrod1")
    return Colour_RGBA(255, 193, 37);
  if (x11colour == "goldenrod2")
    return Colour_RGBA(238, 180, 34);
  if (x11colour == "goldenrod3")
    return Colour_RGBA(205, 155, 29);
  if (x11colour == "goldenrod4")
    return Colour_RGBA(139, 105, 20);
  if (x11colour == "darkgoldenrod1")
    return Colour_RGBA(255, 185, 15);
  if (x11colour == "darkgoldenrod2")
    return Colour_RGBA(238, 173, 14);
  if (x11colour == "darkgoldenrod3")
    return Colour_RGBA(205, 149, 12);
  if (x11colour == "darkgoldenrod4")
    return Colour_RGBA(139, 101, 8);
  if (x11colour == "rosybrown1")
    return Colour_RGBA(255, 193, 193);
  if (x11colour == "rosybrown2")
    return Colour_RGBA(238, 180, 180);
  if (x11colour == "rosybrown3")
    return Colour_RGBA(205, 155, 155);
  if (x11colour == "rosybrown4")
    return Colour_RGBA(139, 105, 105);
  if (x11colour == "indianred1")
    return Colour_RGBA(255, 106, 106);
  if (x11colour == "indianred2")
    return Colour_RGBA(238, 99, 99);
  if (x11colour == "indianred3")
    return Colour_RGBA(205, 85, 85);
  if (x11colour == "indianred4")
    return Colour_RGBA(139, 58, 58);
  if (x11colour == "sienna1")
    return Colour_RGBA(255, 130, 71);
  if (x11colour == "sienna2")
    return Colour_RGBA(238, 121, 66);
  if (x11colour == "sienna3")
    return Colour_RGBA(205, 104, 57);
  if (x11colour == "sienna4")
    return Colour_RGBA(139, 71, 38);
  if (x11colour == "burlywood1")
    return Colour_RGBA(255, 211, 155);
  if (x11colour == "burlywood2")
    return Colour_RGBA(238, 197, 145);
  if (x11colour == "burlywood3")
    return Colour_RGBA(205, 170, 125);
  if (x11colour == "burlywood4")
    return Colour_RGBA(139, 115, 85);
  if (x11colour == "wheat1")
    return Colour_RGBA(255, 231, 186);
  if (x11colour == "wheat2")
    return Colour_RGBA(238, 216, 174);
  if (x11colour == "wheat3")
    return Colour_RGBA(205, 186, 150);
  if (x11colour == "wheat4")
    return Colour_RGBA(139, 126, 102);
  if (x11colour == "tan1")
    return Colour_RGBA(255, 165, 79);
  if (x11colour == "tan2")
    return Colour_RGBA(238, 154, 73);
  if (x11colour == "tan3")
    return Colour_RGBA(205, 133, 63);
  if (x11colour == "tan4")
    return Colour_RGBA(139, 90, 43);
  if (x11colour == "chocolate1")
    return Colour_RGBA(255, 127, 36);
  if (x11colour == "chocolate2")
    return Colour_RGBA(238, 118, 33);
  if (x11colour == "chocolate3")
    return Colour_RGBA(205, 102, 29);
  if (x11colour == "chocolate4")
    return Colour_RGBA(139, 69, 19);
  if (x11colour == "firebrick1")
    return Colour_RGBA(255, 48, 48);
  if (x11colour == "firebrick2")
    return Colour_RGBA(238, 44, 44);
  if (x11colour == "firebrick3")
    return Colour_RGBA(205, 38, 38);
  if (x11colour == "firebrick4")
    return Colour_RGBA(139, 26, 26);
  if (x11colour == "brown1")
    return Colour_RGBA(255, 64, 64);
  if (x11colour == "brown2")
    return Colour_RGBA(238, 59, 59);
  if (x11colour == "brown3")
    return Colour_RGBA(205, 51, 51);
  if (x11colour == "brown4")
    return Colour_RGBA(139, 35, 35);
  if (x11colour == "salmon1")
    return Colour_RGBA(255, 140, 105);
  if (x11colour == "salmon2")
    return Colour_RGBA(238, 130, 98);
  if (x11colour == "salmon3")
    return Colour_RGBA(205, 112, 84);
  if (x11colour == "salmon4")
    return Colour_RGBA(139, 76, 57);
  if (x11colour == "lightsalmon1")
    return Colour_RGBA(255, 160, 122);
  if (x11colour == "lightsalmon2")
    return Colour_RGBA(238, 149, 114);
  if (x11colour == "lightsalmon3")
    return Colour_RGBA(205, 129, 98);
  if (x11colour == "lightsalmon4")
    return Colour_RGBA(139, 87, 66);
  if (x11colour == "orange1")
    return Colour_RGBA(255, 165, 0);
  if (x11colour == "orange2")
    return Colour_RGBA(238, 154, 0);
  if (x11colour == "orange3")
    return Colour_RGBA(205, 133, 0);
  if (x11colour == "orange4")
    return Colour_RGBA(139, 90, 0);
  if (x11colour == "darkorange1")
    return Colour_RGBA(255, 127, 0);
  if (x11colour == "darkorange2")
    return Colour_RGBA(238, 118, 0);
  if (x11colour == "darkorange3")
    return Colour_RGBA(205, 102, 0);
  if (x11colour == "darkorange4")
    return Colour_RGBA(139, 69, 0);
  if (x11colour == "coral1")
    return Colour_RGBA(255, 114, 86);
  if (x11colour == "coral2")
    return Colour_RGBA(238, 106, 80);
  if (x11colour == "coral3")
    return Colour_RGBA(205, 91, 69);
  if (x11colour == "coral4")
    return Colour_RGBA(139, 62, 47);
  if (x11colour == "tomato1")
    return Colour_RGBA(255, 99, 71);
  if (x11colour == "tomato2")
    return Colour_RGBA(238, 92, 66);
  if (x11colour == "tomato3")
    return Colour_RGBA(205, 79, 57);
  if (x11colour == "tomato4")
    return Colour_RGBA(139, 54, 38);
  if (x11colour == "orangered1")
    return Colour_RGBA(255, 69, 0);
  if (x11colour == "orangered2")
    return Colour_RGBA(238, 64, 0);
  if (x11colour == "orangered3")
    return Colour_RGBA(205, 55, 0);
  if (x11colour == "orangered4")
    return Colour_RGBA(139, 37, 0);
  if (x11colour == "red1")
    return Colour_RGBA(255, 0, 0);
  if (x11colour == "red2")
    return Colour_RGBA(238, 0, 0);
  if (x11colour == "red3")
    return Colour_RGBA(205, 0, 0);
  if (x11colour == "red4")
    return Colour_RGBA(139, 0, 0);
  if (x11colour == "deeppink1")
    return Colour_RGBA(255, 20, 147);
  if (x11colour == "deeppink2")
    return Colour_RGBA(238, 18, 137);
  if (x11colour == "deeppink3")
    return Colour_RGBA(205, 16, 118);
  if (x11colour == "deeppink4")
    return Colour_RGBA(139, 10, 80);
  if (x11colour == "hotpink1")
    return Colour_RGBA(255, 110, 180);
  if (x11colour == "hotpink2")
    return Colour_RGBA(238, 106, 167);
  if (x11colour == "hotpink3")
    return Colour_RGBA(205, 96, 144);
  if (x11colour == "hotpink4")
    return Colour_RGBA(139, 58, 98);
  if (x11colour == "pink1")
    return Colour_RGBA(255, 181, 197);
  if (x11colour == "pink2")
    return Colour_RGBA(238, 169, 184);
  if (x11colour == "pink3")
    return Colour_RGBA(205, 145, 158);
  if (x11colour == "pink4")
    return Colour_RGBA(139, 99, 108);
  if (x11colour == "lightpink1")
    return Colour_RGBA(255, 174, 185);
  if (x11colour == "lightpink2")
    return Colour_RGBA(238, 162, 173);
  if (x11colour == "lightpink3")
    return Colour_RGBA(205, 140, 149);
  if (x11colour == "lightpink4")
    return Colour_RGBA(139, 95, 101);
  if (x11colour == "palevioletred1")
    return Colour_RGBA(255, 130, 171);
  if (x11colour == "palevioletred2")
    return Colour_RGBA(238, 121, 159);
  if (x11colour == "palevioletred3")
    return Colour_RGBA(205, 104, 137);
  if (x11colour == "palevioletred4")
    return Colour_RGBA(139, 71, 93);
  if (x11colour == "maroon1")
    return Colour_RGBA(255, 52, 179);
  if (x11colour == "maroon2")
    return Colour_RGBA(238, 48, 167);
  if (x11colour == "maroon3")
    return Colour_RGBA(205, 41, 144);
  if (x11colour == "maroon4")
    return Colour_RGBA(139, 28, 98);
  if (x11colour == "violetred1")
    return Colour_RGBA(255, 62, 150);
  if (x11colour == "violetred2")
    return Colour_RGBA(238, 58, 140);
  if (x11colour == "violetred3")
    return Colour_RGBA(205, 50, 120);
  if (x11colour == "violetred4")
    return Colour_RGBA(139, 34, 82);
  if (x11colour == "magenta1")
    return Colour_RGBA(255, 0, 255);
  if (x11colour == "magenta2")
    return Colour_RGBA(238, 0, 238);
  if (x11colour == "magenta3")
    return Colour_RGBA(205, 0, 205);
  if (x11colour == "magenta4")
    return Colour_RGBA(139, 0, 139);
  if (x11colour == "orchid1")
    return Colour_RGBA(255, 131, 250);
  if (x11colour == "orchid2")
    return Colour_RGBA(238, 122, 233);
  if (x11colour == "orchid3")
    return Colour_RGBA(205, 105, 201);
  if (x11colour == "orchid4")
    return Colour_RGBA(139, 71, 137);
  if (x11colour == "plum1")
    return Colour_RGBA(255, 187, 255);
  if (x11colour == "plum2")
    return Colour_RGBA(238, 174, 238);
  if (x11colour == "plum3")
    return Colour_RGBA(205, 150, 205);
  if (x11colour == "plum4")
    return Colour_RGBA(139, 102, 139);
  if (x11colour == "mediumorchid1")
    return Colour_RGBA(224, 102, 255);
  if (x11colour == "mediumorchid2")
    return Colour_RGBA(209, 95, 238);
  if (x11colour == "mediumorchid3")
    return Colour_RGBA(180, 82, 205);
  if (x11colour == "mediumorchid4")
    return Colour_RGBA(122, 55, 139);
  if (x11colour == "darkorchid1")
    return Colour_RGBA(191, 62, 255);
  if (x11colour == "darkorchid2")
    return Colour_RGBA(178, 58, 238);
  if (x11colour == "darkorchid3")
    return Colour_RGBA(154, 50, 205);
  if (x11colour == "darkorchid4")
    return Colour_RGBA(104, 34, 139);
  if (x11colour == "purple1")
    return Colour_RGBA(155, 48, 255);
  if (x11colour == "purple2")
    return Colour_RGBA(145, 44, 238);
  if (x11colour == "purple3")
    return Colour_RGBA(125, 38, 205);
  if (x11colour == "purple4")
    return Colour_RGBA(85, 26, 139);
  if (x11colour == "mediumpurple1")
    return Colour_RGBA(171, 130, 255);
  if (x11colour == "mediumpurple2")
    return Colour_RGBA(159, 121, 238);
  if (x11colour == "mediumpurple3")
    return Colour_RGBA(137, 104, 205);
  if (x11colour == "mediumpurple4")
    return Colour_RGBA(93, 71, 139);
  if (x11colour == "thistle1")
    return Colour_RGBA(255, 225, 255);
  if (x11colour == "thistle2")
    return Colour_RGBA(238, 210, 238);
  if (x11colour == "thistle3")
    return Colour_RGBA(205, 181, 205);
  if (x11colour == "thistle4")
    return Colour_RGBA(139, 123, 139);
  if (x11colour == "darkgrey")
    return Colour_RGBA(169, 169, 169);
  if (x11colour == "darkgray")
    return Colour_RGBA(169, 169, 169);
  if (x11colour == "darkblue")
    return Colour_RGBA(0, 0, 139);
  if (x11colour == "darkcyan")
    return Colour_RGBA(0, 139, 139);
  if (x11colour == "darkmagenta")
    return Colour_RGBA(139, 0, 139);
  if (x11colour == "darkred")
    return Colour_RGBA(139, 0, 0);
  if (x11colour == "lightgreen")
    return Colour_RGBA(144, 238, 144);
  if (x11colour.substr(0, 4) == "grey" ||
      x11colour.substr(0, 4) == "gray")
  {
    std::stringstream ss(x11colour.substr(4, 2));
    float level;
    if (ss >> level)
    {
      level = floor(level*2.55 + 0.5);
      return Colour_RGBA(level, level, level);
    }
  }

  // default is black
  return Colour_RGBA(0, 0, 0);
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
  std::string red = hexName.substr(1, 2);
  std::string grn = hexName.substr(3, 2);
  std::string blu = hexName.substr(5, 2);

  /* Read colours */
  unsigned int hex;
  sscanf(red.c_str(), "%x", &hex);
  colour.r = hex;
  sscanf(grn.c_str(), "%x", &hex);
  colour.g = hex;
  sscanf(blu.c_str(), "%x", &hex);
  colour.b = hex;

  return colour;
}

