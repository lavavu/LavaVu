#include "GraphicsUtil.h"
#include "Colours.h"

std::ostream & operator<<(std::ostream &os, const Colour& colour)
{
  return os << "rgba(" << (int)colour.r << "," << (int)colour.g << "," << (int)colour.b << "," << _CTOF(colour.a) << ")";
}

void Colour::invert()
{
  int alpha = value & 0xff000000;
  value = (~value & 0x00ffffff) | alpha;
}

json Colour::toJson()
{
  //Default storage type is simple array [R,G,B,A] [0,1]
  json array = {_CTOF(r), _CTOF(g), _CTOF(b), _CTOF(a)};
  //Default storage type HTML style rgba(R[0,255], G[0,255], B[0,255], A[0,1])
  //json array = {r, g, b, _CTOF(a)};
  return array;
}

std::string Colour::toString()
{
  std::stringstream cs;
  cs << *this;
  return cs.str();
}

void Colour::toArray(float* array)
{
  array[0] = _CTOF(r);
  array[1] = _CTOF(g);
  array[2] = _CTOF(b);
  array[3] = _CTOF(a);
}

Colour::Colour(json& jvalue, GLubyte red, GLubyte grn, GLubyte blu, GLubyte alpha) : r(red), g(grn), b(blu), a(alpha)
{
  fromJSON(jvalue);
}


Colour::Colour(const std::string& str)
{
  // Parse without exceptions - required for emscripten
  json jval = json::parse(str, nullptr, false);
  if (jval.is_discarded())
    fromString(str);
  else
    fromJSON(jval);
}

void Colour::fromJSON(json& jvalue)
{
  //Will accept integer colour or [r,g,b,a] array or 
  // string containing x11 name or hex #rrggbb or html rgba(r,g,b,a) 
  try
  {
    if (jvalue.is_number())
    {
      value = jvalue;
    }
    else if (jvalue.is_array())
    {
      float R = jvalue[0];
      float G = jvalue[1];
      float B = jvalue[2];
      float A = 1.0;
      if (jvalue.size() > 3) A = jvalue[3];
      if (R <= 1.0 && G <= 1.0 && B <= 1.0)
      {
        r = _FTOC(R);
        g = _FTOC(G);
        b = _FTOC(B);
      }
      else
      {
        r = R;
        g = G;
        b = B;
      }

      //Parse alpha separately, allows [0-255] RGB + [0,1] A
      a = A <= 1.0 ? _FTOC(A) : A;
    }
    else if (jvalue.is_string())
    {
      fromString(jvalue);
    }
    //std::cout << "COLOUR: " << std::setw(2) << jvalue << " ==> " << *this << std::endl;
  }
  catch (std::exception& e)
  {
    std::cerr << "Failed to parse json colour: " << jvalue << " : " << e.what() << std::endl;
  }
}

void Colour::fromString(const std::string& str)
{
  if (str.find("rgb") != std::string::npos) 
  {
    //Parse HTML format rgba(r,g,b,a) or rgb(r,g,b) values
    //RGB all [0,255], Alpha can be [0-255] or [0,1]
    bool hasalpha = str.find("rgba(") != std::string::npos;
    int start = hasalpha ? 5 : 4;
    int c;
    float alpha;
    try
    {
      std::stringstream ss(str.substr(start));
      for (int i=0; i<3; i++)
      {
        ss >> c;
        rgba[i] = c;
        char next = ss.peek();
        if (next == ',' || next == ' ')
          ss.ignore();
      }

      if (hasalpha)
      {
        ss >> alpha;
        if (alpha > 1.)
          a = alpha;
        else
          a = 255 * alpha;
      }
      else
        a = 255;
    }
    catch (std::exception& e)
    {
      std::cerr << "Failed to parse rgba colour: " << str << " : " << e.what() << std::endl;
    }
  }
  else
  {
    //Parses X11 colour names or hex #RRGGBB with
    // optional alpha indicated by colon then value [0,1]
    float opacity = 1.0;
    size_t pos = str.find(":");
    std::string sstr = str;
    if (pos != std::string::npos)
    {
      std::stringstream ss(str.substr(pos+1));
      if (!(ss >> opacity))
        opacity = 1.0;
      sstr = str.substr(0, pos);
    }

    fromX11Colour(sstr);

    a = opacity * 255;
  }
}

// Reads hex or colour from X11 colour list
// Defaults to black
void Colour::fromX11Colour(std::string x11colour)
{
  std::transform(x11colour.begin(), x11colour.end(), x11colour.begin(), ::tolower);
  if (x11colour.at(0) == '#')
    return fromHex(x11colour);
  if (x11colour == "snow")
    return fromRGBA(255, 250, 250);
  if (x11colour == "ghostwhite")
    return fromRGBA(248, 248, 255);
  if (x11colour == "whitesmoke")
    return fromRGBA(245, 245, 245);
  if (x11colour == "gainsboro")
    return fromRGBA(220, 220, 220);
  if (x11colour == "floralwhite")
    return fromRGBA(255, 250, 240);
  if (x11colour == "oldlace")
    return fromRGBA(253, 245, 230);
  if (x11colour == "linen")
    return fromRGBA(250, 240, 230);
  if (x11colour == "antiquewhite")
    return fromRGBA(250, 235, 215);
  if (x11colour == "papayawhip")
    return fromRGBA(255, 239, 213);
  if (x11colour == "blanchedalmond")
    return fromRGBA(255, 235, 205);
  if (x11colour == "bisque")
    return fromRGBA(255, 228, 196);
  if (x11colour == "peachpuff")
    return fromRGBA(255, 218, 185);
  if (x11colour == "navajowhite")
    return fromRGBA(255, 222, 173);
  if (x11colour == "moccasin")
    return fromRGBA(255, 228, 181);
  if (x11colour == "cornsilk")
    return fromRGBA(255, 248, 220);
  if (x11colour == "ivory")
    return fromRGBA(255, 255, 240);
  if (x11colour == "lemonchiffon")
    return fromRGBA(255, 250, 205);
  if (x11colour == "seashell")
    return fromRGBA(255, 245, 238);
  if (x11colour == "honeydew")
    return fromRGBA(240, 255, 240);
  if (x11colour == "mintcream")
    return fromRGBA(245, 255, 250);
  if (x11colour == "azure")
    return fromRGBA(240, 255, 255);
  if (x11colour == "aliceblue")
    return fromRGBA(240, 248, 255);
  if (x11colour == "lavender")
    return fromRGBA(230, 230, 250);
  if (x11colour == "lavenderblush")
    return fromRGBA(255, 240, 245);
  if (x11colour == "mistyrose")
    return fromRGBA(255, 228, 225);
  if (x11colour == "white")
    return fromRGBA(255, 255, 255);
  if (x11colour == "black")
    return fromRGBA(0, 0, 0);
  if (x11colour == "darkslategray")
    return fromRGBA(47, 79, 79);
  if (x11colour == "darkslategrey")
    return fromRGBA(47, 79, 79);
  if (x11colour == "dimgray")
    return fromRGBA(105, 105, 105);
  if (x11colour == "dimgrey")
    return fromRGBA(105, 105, 105);
  if (x11colour == "slategray")
    return fromRGBA(112, 128, 144);
  if (x11colour == "slategrey")
    return fromRGBA(112, 128, 144);
  if (x11colour == "lightslategray")
    return fromRGBA(119, 136, 153);
  if (x11colour == "lightslategrey")
    return fromRGBA(119, 136, 153);
  if (x11colour == "gray")
    return fromRGBA(190, 190, 190);
  if (x11colour == "grey")
    return fromRGBA(190, 190, 190);
  if (x11colour == "lightgrey")
    return fromRGBA(211, 211, 211);
  if (x11colour == "lightgray")
    return fromRGBA(211, 211, 211);
  if (x11colour == "midnightblue")
    return fromRGBA(25, 25, 112);
  if (x11colour == "navy")
    return fromRGBA(0, 0, 128);
  if (x11colour == "navyblue")
    return fromRGBA(0, 0, 128);
  if (x11colour == "cornflowerblue")
    return fromRGBA(100, 149, 237);
  if (x11colour == "darkslateblue")
    return fromRGBA(72, 61, 139);
  if (x11colour == "slateblue")
    return fromRGBA(106, 90, 205);
  if (x11colour == "mediumslateblue")
    return fromRGBA(123, 104, 238);
  if (x11colour == "lightslateblue")
    return fromRGBA(132, 112, 255);
  if (x11colour == "mediumblue")
    return fromRGBA(0, 0, 205);
  if (x11colour == "royalblue")
    return fromRGBA(65, 105, 225);
  if (x11colour == "blue")
    return fromRGBA(0, 0, 255);
  if (x11colour == "dodgerblue")
    return fromRGBA(30, 144, 255);
  if (x11colour == "deepskyblue")
    return fromRGBA(0, 191, 255);
  if (x11colour == "skyblue")
    return fromRGBA(135, 206, 235);
  if (x11colour == "lightskyblue")
    return fromRGBA(135, 206, 250);
  if (x11colour == "steelblue")
    return fromRGBA(70, 130, 180);
  if (x11colour == "lightsteelblue")
    return fromRGBA(176, 196, 222);
  if (x11colour == "lightblue")
    return fromRGBA(173, 216, 230);
  if (x11colour == "powderblue")
    return fromRGBA(176, 224, 230);
  if (x11colour == "paleturquoise")
    return fromRGBA(175, 238, 238);
  if (x11colour == "darkturquoise")
    return fromRGBA(0, 206, 209);
  if (x11colour == "mediumturquoise")
    return fromRGBA(72, 209, 204);
  if (x11colour == "turquoise")
    return fromRGBA(64, 224, 208);
  if (x11colour == "cyan")
    return fromRGBA(0, 255, 255);
  if (x11colour == "lightcyan")
    return fromRGBA(224, 255, 255);
  if (x11colour == "cadetblue")
    return fromRGBA(95, 158, 160);
  if (x11colour == "mediumaquamarine")
    return fromRGBA(102, 205, 170);
  if (x11colour == "aquamarine")
    return fromRGBA(127, 255, 212);
  if (x11colour == "darkgreen")
    return fromRGBA(0, 100, 0);
  if (x11colour == "darkolivegreen")
    return fromRGBA(85, 107, 47);
  if (x11colour == "darkseagreen")
    return fromRGBA(143, 188, 143);
  if (x11colour == "seagreen")
    return fromRGBA(46, 139, 87);
  if (x11colour == "mediumseagreen")
    return fromRGBA(60, 179, 113);
  if (x11colour == "lightseagreen")
    return fromRGBA(32, 178, 170);
  if (x11colour == "palegreen")
    return fromRGBA(152, 251, 152);
  if (x11colour == "springgreen")
    return fromRGBA(0, 255, 127);
  if (x11colour == "lawngreen")
    return fromRGBA(124, 252, 0);
  if (x11colour == "green")
    return fromRGBA(0, 255, 0);
  if (x11colour == "chartreuse")
    return fromRGBA(127, 255, 0);
  if (x11colour == "mediumspringgreen")
    return fromRGBA(0, 250, 154);
  if (x11colour == "greenyellow")
    return fromRGBA(173, 255, 47);
  if (x11colour == "limegreen")
    return fromRGBA(50, 205, 50);
  if (x11colour == "yellowgreen")
    return fromRGBA(154, 205, 50);
  if (x11colour == "forestgreen")
    return fromRGBA(34, 139, 34);
  if (x11colour == "olivedrab")
    return fromRGBA(107, 142, 35);
  if (x11colour == "darkkhaki")
    return fromRGBA(189, 183, 107);
  if (x11colour == "khaki")
    return fromRGBA(240, 230, 140);
  if (x11colour == "palegoldenrod")
    return fromRGBA(238, 232, 170);
  if (x11colour == "lightgoldenrodyellow")
    return fromRGBA(250, 250, 210);
  if (x11colour == "lightyellow")
    return fromRGBA(255, 255, 224);
  if (x11colour == "yellow")
    return fromRGBA(255, 255, 0);
  if (x11colour == "gold")
    return fromRGBA(255, 215, 0);
  if (x11colour == "lightgoldenrod")
    return fromRGBA(238, 221, 130);
  if (x11colour == "goldenrod")
    return fromRGBA(218, 165, 32);
  if (x11colour == "darkgoldenrod")
    return fromRGBA(184, 134, 11);
  if (x11colour == "rosybrown")
    return fromRGBA(188, 143, 143);
  if (x11colour == "indianred")
    return fromRGBA(205, 92, 92);
  if (x11colour == "saddlebrown")
    return fromRGBA(139, 69, 19);
  if (x11colour == "sienna")
    return fromRGBA(160, 82, 45);
  if (x11colour == "peru")
    return fromRGBA(205, 133, 63);
  if (x11colour == "burlywood")
    return fromRGBA(222, 184, 135);
  if (x11colour == "beige")
    return fromRGBA(245, 245, 220);
  if (x11colour == "wheat")
    return fromRGBA(245, 222, 179);
  if (x11colour == "sandybrown")
    return fromRGBA(244, 164, 96);
  if (x11colour == "tan")
    return fromRGBA(210, 180, 140);
  if (x11colour == "chocolate")
    return fromRGBA(210, 105, 30);
  if (x11colour == "firebrick")
    return fromRGBA(178, 34, 34);
  if (x11colour == "brown")
    return fromRGBA(165, 42, 42);
  if (x11colour == "darksalmon")
    return fromRGBA(233, 150, 122);
  if (x11colour == "salmon")
    return fromRGBA(250, 128, 114);
  if (x11colour == "lightsalmon")
    return fromRGBA(255, 160, 122);
  if (x11colour == "orange")
    return fromRGBA(255, 165, 0);
  if (x11colour == "darkorange")
    return fromRGBA(255, 140, 0);
  if (x11colour == "coral")
    return fromRGBA(255, 127, 80);
  if (x11colour == "lightcoral")
    return fromRGBA(240, 128, 128);
  if (x11colour == "tomato")
    return fromRGBA(255, 99, 71);
  if (x11colour == "orangered")
    return fromRGBA(255, 69, 0);
  if (x11colour == "red")
    return fromRGBA(255, 0, 0);
  if (x11colour == "hotpink")
    return fromRGBA(255, 105, 180);
  if (x11colour == "deeppink")
    return fromRGBA(255, 20, 147);
  if (x11colour == "pink")
    return fromRGBA(255, 192, 203);
  if (x11colour == "lightpink")
    return fromRGBA(255, 182, 193);
  if (x11colour == "palevioletred")
    return fromRGBA(219, 112, 147);
  if (x11colour == "maroon")
    return fromRGBA(176, 48, 96);
  if (x11colour == "mediumvioletred")
    return fromRGBA(199, 21, 133);
  if (x11colour == "violetred")
    return fromRGBA(208, 32, 144);
  if (x11colour == "magenta")
    return fromRGBA(255, 0, 255);
  if (x11colour == "violet")
    return fromRGBA(238, 130, 238);
  if (x11colour == "plum")
    return fromRGBA(221, 160, 221);
  if (x11colour == "orchid")
    return fromRGBA(218, 112, 214);
  if (x11colour == "mediumorchid")
    return fromRGBA(186, 85, 211);
  if (x11colour == "darkorchid")
    return fromRGBA(153, 50, 204);
  if (x11colour == "darkviolet")
    return fromRGBA(148, 0, 211);
  if (x11colour == "blueviolet")
    return fromRGBA(138, 43, 226);
  if (x11colour == "purple")
    return fromRGBA(160, 32, 240);
  if (x11colour == "mediumpurple")
    return fromRGBA(147, 112, 219);
  if (x11colour == "thistle")
    return fromRGBA(216, 191, 216);
  if (x11colour == "snow1")
    return fromRGBA(255, 250, 250);
  if (x11colour == "snow2")
    return fromRGBA(238, 233, 233);
  if (x11colour == "snow3")
    return fromRGBA(205, 201, 201);
  if (x11colour == "snow4")
    return fromRGBA(139, 137, 137);
  if (x11colour == "seashell1")
    return fromRGBA(255, 245, 238);
  if (x11colour == "seashell2")
    return fromRGBA(238, 229, 222);
  if (x11colour == "seashell3")
    return fromRGBA(205, 197, 191);
  if (x11colour == "seashell4")
    return fromRGBA(139, 134, 130);
  if (x11colour == "antiquewhite1")
    return fromRGBA(255, 239, 219);
  if (x11colour == "antiquewhite2")
    return fromRGBA(238, 223, 204);
  if (x11colour == "antiquewhite3")
    return fromRGBA(205, 192, 176);
  if (x11colour == "antiquewhite4")
    return fromRGBA(139, 131, 120);
  if (x11colour == "bisque1")
    return fromRGBA(255, 228, 196);
  if (x11colour == "bisque2")
    return fromRGBA(238, 213, 183);
  if (x11colour == "bisque3")
    return fromRGBA(205, 183, 158);
  if (x11colour == "bisque4")
    return fromRGBA(139, 125, 107);
  if (x11colour == "peachpuff1")
    return fromRGBA(255, 218, 185);
  if (x11colour == "peachpuff2")
    return fromRGBA(238, 203, 173);
  if (x11colour == "peachpuff3")
    return fromRGBA(205, 175, 149);
  if (x11colour == "peachpuff4")
    return fromRGBA(139, 119, 101);
  if (x11colour == "navajowhite1")
    return fromRGBA(255, 222, 173);
  if (x11colour == "navajowhite2")
    return fromRGBA(238, 207, 161);
  if (x11colour == "navajowhite3")
    return fromRGBA(205, 179, 139);
  if (x11colour == "navajowhite4")
    return fromRGBA(139, 121, 94);
  if (x11colour == "lemonchiffon1")
    return fromRGBA(255, 250, 205);
  if (x11colour == "lemonchiffon2")
    return fromRGBA(238, 233, 191);
  if (x11colour == "lemonchiffon3")
    return fromRGBA(205, 201, 165);
  if (x11colour == "lemonchiffon4")
    return fromRGBA(139, 137, 112);
  if (x11colour == "cornsilk1")
    return fromRGBA(255, 248, 220);
  if (x11colour == "cornsilk2")
    return fromRGBA(238, 232, 205);
  if (x11colour == "cornsilk3")
    return fromRGBA(205, 200, 177);
  if (x11colour == "cornsilk4")
    return fromRGBA(139, 136, 120);
  if (x11colour == "ivory1")
    return fromRGBA(255, 255, 240);
  if (x11colour == "ivory2")
    return fromRGBA(238, 238, 224);
  if (x11colour == "ivory3")
    return fromRGBA(205, 205, 193);
  if (x11colour == "ivory4")
    return fromRGBA(139, 139, 131);
  if (x11colour == "honeydew1")
    return fromRGBA(240, 255, 240);
  if (x11colour == "honeydew2")
    return fromRGBA(224, 238, 224);
  if (x11colour == "honeydew3")
    return fromRGBA(193, 205, 193);
  if (x11colour == "honeydew4")
    return fromRGBA(131, 139, 131);
  if (x11colour == "lavenderblush1")
    return fromRGBA(255, 240, 245);
  if (x11colour == "lavenderblush2")
    return fromRGBA(238, 224, 229);
  if (x11colour == "lavenderblush3")
    return fromRGBA(205, 193, 197);
  if (x11colour == "lavenderblush4")
    return fromRGBA(139, 131, 134);
  if (x11colour == "mistyrose1")
    return fromRGBA(255, 228, 225);
  if (x11colour == "mistyrose2")
    return fromRGBA(238, 213, 210);
  if (x11colour == "mistyrose3")
    return fromRGBA(205, 183, 181);
  if (x11colour == "mistyrose4")
    return fromRGBA(139, 125, 123);
  if (x11colour == "azure1")
    return fromRGBA(240, 255, 255);
  if (x11colour == "azure2")
    return fromRGBA(224, 238, 238);
  if (x11colour == "azure3")
    return fromRGBA(193, 205, 205);
  if (x11colour == "azure4")
    return fromRGBA(131, 139, 139);
  if (x11colour == "slateblue1")
    return fromRGBA(131, 111, 255);
  if (x11colour == "slateblue2")
    return fromRGBA(122, 103, 238);
  if (x11colour == "slateblue3")
    return fromRGBA(105, 89, 205);
  if (x11colour == "slateblue4")
    return fromRGBA(71, 60, 139);
  if (x11colour == "royalblue1")
    return fromRGBA(72, 118, 255);
  if (x11colour == "royalblue2")
    return fromRGBA(67, 110, 238);
  if (x11colour == "royalblue3")
    return fromRGBA(58, 95, 205);
  if (x11colour == "royalblue4")
    return fromRGBA(39, 64, 139);
  if (x11colour == "blue1")
    return fromRGBA(0, 0, 255);
  if (x11colour == "blue2")
    return fromRGBA(0, 0, 238);
  if (x11colour == "blue3")
    return fromRGBA(0, 0, 205);
  if (x11colour == "blue4")
    return fromRGBA(0, 0, 139);
  if (x11colour == "dodgerblue1")
    return fromRGBA(30, 144, 255);
  if (x11colour == "dodgerblue2")
    return fromRGBA(28, 134, 238);
  if (x11colour == "dodgerblue3")
    return fromRGBA(24, 116, 205);
  if (x11colour == "dodgerblue4")
    return fromRGBA(16, 78, 139);
  if (x11colour == "steelblue1")
    return fromRGBA(99, 184, 255);
  if (x11colour == "steelblue2")
    return fromRGBA(92, 172, 238);
  if (x11colour == "steelblue3")
    return fromRGBA(79, 148, 205);
  if (x11colour == "steelblue4")
    return fromRGBA(54, 100, 139);
  if (x11colour == "deepskyblue1")
    return fromRGBA(0, 191, 255);
  if (x11colour == "deepskyblue2")
    return fromRGBA(0, 178, 238);
  if (x11colour == "deepskyblue3")
    return fromRGBA(0, 154, 205);
  if (x11colour == "deepskyblue4")
    return fromRGBA(0, 104, 139);
  if (x11colour == "skyblue1")
    return fromRGBA(135, 206, 255);
  if (x11colour == "skyblue2")
    return fromRGBA(126, 192, 238);
  if (x11colour == "skyblue3")
    return fromRGBA(108, 166, 205);
  if (x11colour == "skyblue4")
    return fromRGBA(74, 112, 139);
  if (x11colour == "lightskyblue1")
    return fromRGBA(176, 226, 255);
  if (x11colour == "lightskyblue2")
    return fromRGBA(164, 211, 238);
  if (x11colour == "lightskyblue3")
    return fromRGBA(141, 182, 205);
  if (x11colour == "lightskyblue4")
    return fromRGBA(96, 123, 139);
  if (x11colour == "slategray1")
    return fromRGBA(198, 226, 255);
  if (x11colour == "slategray2")
    return fromRGBA(185, 211, 238);
  if (x11colour == "slategray3")
    return fromRGBA(159, 182, 205);
  if (x11colour == "slategray4")
    return fromRGBA(108, 123, 139);
  if (x11colour == "lightsteelblue1")
    return fromRGBA(202, 225, 255);
  if (x11colour == "lightsteelblue2")
    return fromRGBA(188, 210, 238);
  if (x11colour == "lightsteelblue3")
    return fromRGBA(162, 181, 205);
  if (x11colour == "lightsteelblue4")
    return fromRGBA(110, 123, 139);
  if (x11colour == "lightblue1")
    return fromRGBA(191, 239, 255);
  if (x11colour == "lightblue2")
    return fromRGBA(178, 223, 238);
  if (x11colour == "lightblue3")
    return fromRGBA(154, 192, 205);
  if (x11colour == "lightblue4")
    return fromRGBA(104, 131, 139);
  if (x11colour == "lightcyan1")
    return fromRGBA(224, 255, 255);
  if (x11colour == "lightcyan2")
    return fromRGBA(209, 238, 238);
  if (x11colour == "lightcyan3")
    return fromRGBA(180, 205, 205);
  if (x11colour == "lightcyan4")
    return fromRGBA(122, 139, 139);
  if (x11colour == "paleturquoise1")
    return fromRGBA(187, 255, 255);
  if (x11colour == "paleturquoise2")
    return fromRGBA(174, 238, 238);
  if (x11colour == "paleturquoise3")
    return fromRGBA(150, 205, 205);
  if (x11colour == "paleturquoise4")
    return fromRGBA(102, 139, 139);
  if (x11colour == "cadetblue1")
    return fromRGBA(152, 245, 255);
  if (x11colour == "cadetblue2")
    return fromRGBA(142, 229, 238);
  if (x11colour == "cadetblue3")
    return fromRGBA(122, 197, 205);
  if (x11colour == "cadetblue4")
    return fromRGBA(83, 134, 139);
  if (x11colour == "turquoise1")
    return fromRGBA(0, 245, 255);
  if (x11colour == "turquoise2")
    return fromRGBA(0, 229, 238);
  if (x11colour == "turquoise3")
    return fromRGBA(0, 197, 205);
  if (x11colour == "turquoise4")
    return fromRGBA(0, 134, 139);
  if (x11colour == "cyan1")
    return fromRGBA(0, 255, 255);
  if (x11colour == "cyan2")
    return fromRGBA(0, 238, 238);
  if (x11colour == "cyan3")
    return fromRGBA(0, 205, 205);
  if (x11colour == "cyan4")
    return fromRGBA(0, 139, 139);
  if (x11colour == "darkslategray1")
    return fromRGBA(151, 255, 255);
  if (x11colour == "darkslategray2")
    return fromRGBA(141, 238, 238);
  if (x11colour == "darkslategray3")
    return fromRGBA(121, 205, 205);
  if (x11colour == "darkslategray4")
    return fromRGBA(82, 139, 139);
  if (x11colour == "aquamarine1")
    return fromRGBA(127, 255, 212);
  if (x11colour == "aquamarine2")
    return fromRGBA(118, 238, 198);
  if (x11colour == "aquamarine3")
    return fromRGBA(102, 205, 170);
  if (x11colour == "aquamarine4")
    return fromRGBA(69, 139, 116);
  if (x11colour == "darkseagreen1")
    return fromRGBA(193, 255, 193);
  if (x11colour == "darkseagreen2")
    return fromRGBA(180, 238, 180);
  if (x11colour == "darkseagreen3")
    return fromRGBA(155, 205, 155);
  if (x11colour == "darkseagreen4")
    return fromRGBA(105, 139, 105);
  if (x11colour == "seagreen1")
    return fromRGBA(84, 255, 159);
  if (x11colour == "seagreen2")
    return fromRGBA(78, 238, 148);
  if (x11colour == "seagreen3")
    return fromRGBA(67, 205, 128);
  if (x11colour == "seagreen4")
    return fromRGBA(46, 139, 87);
  if (x11colour == "palegreen1")
    return fromRGBA(154, 255, 154);
  if (x11colour == "palegreen2")
    return fromRGBA(144, 238, 144);
  if (x11colour == "palegreen3")
    return fromRGBA(124, 205, 124);
  if (x11colour == "palegreen4")
    return fromRGBA(84, 139, 84);
  if (x11colour == "springgreen1")
    return fromRGBA(0, 255, 127);
  if (x11colour == "springgreen2")
    return fromRGBA(0, 238, 118);
  if (x11colour == "springgreen3")
    return fromRGBA(0, 205, 102);
  if (x11colour == "springgreen4")
    return fromRGBA(0, 139, 69);
  if (x11colour == "green1")
    return fromRGBA(0, 255, 0);
  if (x11colour == "green2")
    return fromRGBA(0, 238, 0);
  if (x11colour == "green3")
    return fromRGBA(0, 205, 0);
  if (x11colour == "green4")
    return fromRGBA(0, 139, 0);
  if (x11colour == "chartreuse1")
    return fromRGBA(127, 255, 0);
  if (x11colour == "chartreuse2")
    return fromRGBA(118, 238, 0);
  if (x11colour == "chartreuse3")
    return fromRGBA(102, 205, 0);
  if (x11colour == "chartreuse4")
    return fromRGBA(69, 139, 0);
  if (x11colour == "olivedrab1")
    return fromRGBA(192, 255, 62);
  if (x11colour == "olivedrab2")
    return fromRGBA(179, 238, 58);
  if (x11colour == "olivedrab3")
    return fromRGBA(154, 205, 50);
  if (x11colour == "olivedrab4")
    return fromRGBA(105, 139, 34);
  if (x11colour == "darkolivegreen1")
    return fromRGBA(202, 255, 112);
  if (x11colour == "darkolivegreen2")
    return fromRGBA(188, 238, 104);
  if (x11colour == "darkolivegreen3")
    return fromRGBA(162, 205, 90);
  if (x11colour == "darkolivegreen4")
    return fromRGBA(110, 139, 61);
  if (x11colour == "khaki1")
    return fromRGBA(255, 246, 143);
  if (x11colour == "khaki2")
    return fromRGBA(238, 230, 133);
  if (x11colour == "khaki3")
    return fromRGBA(205, 198, 115);
  if (x11colour == "khaki4")
    return fromRGBA(139, 134, 78);
  if (x11colour == "lightgoldenrod1")
    return fromRGBA(255, 236, 139);
  if (x11colour == "lightgoldenrod2")
    return fromRGBA(238, 220, 130);
  if (x11colour == "lightgoldenrod3")
    return fromRGBA(205, 190, 112);
  if (x11colour == "lightgoldenrod4")
    return fromRGBA(139, 129, 76);
  if (x11colour == "lightyellow1")
    return fromRGBA(255, 255, 224);
  if (x11colour == "lightyellow2")
    return fromRGBA(238, 238, 209);
  if (x11colour == "lightyellow3")
    return fromRGBA(205, 205, 180);
  if (x11colour == "lightyellow4")
    return fromRGBA(139, 139, 122);
  if (x11colour == "yellow1")
    return fromRGBA(255, 255, 0);
  if (x11colour == "yellow2")
    return fromRGBA(238, 238, 0);
  if (x11colour == "yellow3")
    return fromRGBA(205, 205, 0);
  if (x11colour == "yellow4")
    return fromRGBA(139, 139, 0);
  if (x11colour == "gold1")
    return fromRGBA(255, 215, 0);
  if (x11colour == "gold2")
    return fromRGBA(238, 201, 0);
  if (x11colour == "gold3")
    return fromRGBA(205, 173, 0);
  if (x11colour == "gold4")
    return fromRGBA(139, 117, 0);
  if (x11colour == "goldenrod1")
    return fromRGBA(255, 193, 37);
  if (x11colour == "goldenrod2")
    return fromRGBA(238, 180, 34);
  if (x11colour == "goldenrod3")
    return fromRGBA(205, 155, 29);
  if (x11colour == "goldenrod4")
    return fromRGBA(139, 105, 20);
  if (x11colour == "darkgoldenrod1")
    return fromRGBA(255, 185, 15);
  if (x11colour == "darkgoldenrod2")
    return fromRGBA(238, 173, 14);
  if (x11colour == "darkgoldenrod3")
    return fromRGBA(205, 149, 12);
  if (x11colour == "darkgoldenrod4")
    return fromRGBA(139, 101, 8);
  if (x11colour == "rosybrown1")
    return fromRGBA(255, 193, 193);
  if (x11colour == "rosybrown2")
    return fromRGBA(238, 180, 180);
  if (x11colour == "rosybrown3")
    return fromRGBA(205, 155, 155);
  if (x11colour == "rosybrown4")
    return fromRGBA(139, 105, 105);
  if (x11colour == "indianred1")
    return fromRGBA(255, 106, 106);
  if (x11colour == "indianred2")
    return fromRGBA(238, 99, 99);
  if (x11colour == "indianred3")
    return fromRGBA(205, 85, 85);
  if (x11colour == "indianred4")
    return fromRGBA(139, 58, 58);
  if (x11colour == "sienna1")
    return fromRGBA(255, 130, 71);
  if (x11colour == "sienna2")
    return fromRGBA(238, 121, 66);
  if (x11colour == "sienna3")
    return fromRGBA(205, 104, 57);
  if (x11colour == "sienna4")
    return fromRGBA(139, 71, 38);
  if (x11colour == "burlywood1")
    return fromRGBA(255, 211, 155);
  if (x11colour == "burlywood2")
    return fromRGBA(238, 197, 145);
  if (x11colour == "burlywood3")
    return fromRGBA(205, 170, 125);
  if (x11colour == "burlywood4")
    return fromRGBA(139, 115, 85);
  if (x11colour == "wheat1")
    return fromRGBA(255, 231, 186);
  if (x11colour == "wheat2")
    return fromRGBA(238, 216, 174);
  if (x11colour == "wheat3")
    return fromRGBA(205, 186, 150);
  if (x11colour == "wheat4")
    return fromRGBA(139, 126, 102);
  if (x11colour == "tan1")
    return fromRGBA(255, 165, 79);
  if (x11colour == "tan2")
    return fromRGBA(238, 154, 73);
  if (x11colour == "tan3")
    return fromRGBA(205, 133, 63);
  if (x11colour == "tan4")
    return fromRGBA(139, 90, 43);
  if (x11colour == "chocolate1")
    return fromRGBA(255, 127, 36);
  if (x11colour == "chocolate2")
    return fromRGBA(238, 118, 33);
  if (x11colour == "chocolate3")
    return fromRGBA(205, 102, 29);
  if (x11colour == "chocolate4")
    return fromRGBA(139, 69, 19);
  if (x11colour == "firebrick1")
    return fromRGBA(255, 48, 48);
  if (x11colour == "firebrick2")
    return fromRGBA(238, 44, 44);
  if (x11colour == "firebrick3")
    return fromRGBA(205, 38, 38);
  if (x11colour == "firebrick4")
    return fromRGBA(139, 26, 26);
  if (x11colour == "brown1")
    return fromRGBA(255, 64, 64);
  if (x11colour == "brown2")
    return fromRGBA(238, 59, 59);
  if (x11colour == "brown3")
    return fromRGBA(205, 51, 51);
  if (x11colour == "brown4")
    return fromRGBA(139, 35, 35);
  if (x11colour == "salmon1")
    return fromRGBA(255, 140, 105);
  if (x11colour == "salmon2")
    return fromRGBA(238, 130, 98);
  if (x11colour == "salmon3")
    return fromRGBA(205, 112, 84);
  if (x11colour == "salmon4")
    return fromRGBA(139, 76, 57);
  if (x11colour == "lightsalmon1")
    return fromRGBA(255, 160, 122);
  if (x11colour == "lightsalmon2")
    return fromRGBA(238, 149, 114);
  if (x11colour == "lightsalmon3")
    return fromRGBA(205, 129, 98);
  if (x11colour == "lightsalmon4")
    return fromRGBA(139, 87, 66);
  if (x11colour == "orange1")
    return fromRGBA(255, 165, 0);
  if (x11colour == "orange2")
    return fromRGBA(238, 154, 0);
  if (x11colour == "orange3")
    return fromRGBA(205, 133, 0);
  if (x11colour == "orange4")
    return fromRGBA(139, 90, 0);
  if (x11colour == "darkorange1")
    return fromRGBA(255, 127, 0);
  if (x11colour == "darkorange2")
    return fromRGBA(238, 118, 0);
  if (x11colour == "darkorange3")
    return fromRGBA(205, 102, 0);
  if (x11colour == "darkorange4")
    return fromRGBA(139, 69, 0);
  if (x11colour == "coral1")
    return fromRGBA(255, 114, 86);
  if (x11colour == "coral2")
    return fromRGBA(238, 106, 80);
  if (x11colour == "coral3")
    return fromRGBA(205, 91, 69);
  if (x11colour == "coral4")
    return fromRGBA(139, 62, 47);
  if (x11colour == "tomato1")
    return fromRGBA(255, 99, 71);
  if (x11colour == "tomato2")
    return fromRGBA(238, 92, 66);
  if (x11colour == "tomato3")
    return fromRGBA(205, 79, 57);
  if (x11colour == "tomato4")
    return fromRGBA(139, 54, 38);
  if (x11colour == "orangered1")
    return fromRGBA(255, 69, 0);
  if (x11colour == "orangered2")
    return fromRGBA(238, 64, 0);
  if (x11colour == "orangered3")
    return fromRGBA(205, 55, 0);
  if (x11colour == "orangered4")
    return fromRGBA(139, 37, 0);
  if (x11colour == "red1")
    return fromRGBA(255, 0, 0);
  if (x11colour == "red2")
    return fromRGBA(238, 0, 0);
  if (x11colour == "red3")
    return fromRGBA(205, 0, 0);
  if (x11colour == "red4")
    return fromRGBA(139, 0, 0);
  if (x11colour == "deeppink1")
    return fromRGBA(255, 20, 147);
  if (x11colour == "deeppink2")
    return fromRGBA(238, 18, 137);
  if (x11colour == "deeppink3")
    return fromRGBA(205, 16, 118);
  if (x11colour == "deeppink4")
    return fromRGBA(139, 10, 80);
  if (x11colour == "hotpink1")
    return fromRGBA(255, 110, 180);
  if (x11colour == "hotpink2")
    return fromRGBA(238, 106, 167);
  if (x11colour == "hotpink3")
    return fromRGBA(205, 96, 144);
  if (x11colour == "hotpink4")
    return fromRGBA(139, 58, 98);
  if (x11colour == "pink1")
    return fromRGBA(255, 181, 197);
  if (x11colour == "pink2")
    return fromRGBA(238, 169, 184);
  if (x11colour == "pink3")
    return fromRGBA(205, 145, 158);
  if (x11colour == "pink4")
    return fromRGBA(139, 99, 108);
  if (x11colour == "lightpink1")
    return fromRGBA(255, 174, 185);
  if (x11colour == "lightpink2")
    return fromRGBA(238, 162, 173);
  if (x11colour == "lightpink3")
    return fromRGBA(205, 140, 149);
  if (x11colour == "lightpink4")
    return fromRGBA(139, 95, 101);
  if (x11colour == "palevioletred1")
    return fromRGBA(255, 130, 171);
  if (x11colour == "palevioletred2")
    return fromRGBA(238, 121, 159);
  if (x11colour == "palevioletred3")
    return fromRGBA(205, 104, 137);
  if (x11colour == "palevioletred4")
    return fromRGBA(139, 71, 93);
  if (x11colour == "maroon1")
    return fromRGBA(255, 52, 179);
  if (x11colour == "maroon2")
    return fromRGBA(238, 48, 167);
  if (x11colour == "maroon3")
    return fromRGBA(205, 41, 144);
  if (x11colour == "maroon4")
    return fromRGBA(139, 28, 98);
  if (x11colour == "violetred1")
    return fromRGBA(255, 62, 150);
  if (x11colour == "violetred2")
    return fromRGBA(238, 58, 140);
  if (x11colour == "violetred3")
    return fromRGBA(205, 50, 120);
  if (x11colour == "violetred4")
    return fromRGBA(139, 34, 82);
  if (x11colour == "magenta1")
    return fromRGBA(255, 0, 255);
  if (x11colour == "magenta2")
    return fromRGBA(238, 0, 238);
  if (x11colour == "magenta3")
    return fromRGBA(205, 0, 205);
  if (x11colour == "magenta4")
    return fromRGBA(139, 0, 139);
  if (x11colour == "orchid1")
    return fromRGBA(255, 131, 250);
  if (x11colour == "orchid2")
    return fromRGBA(238, 122, 233);
  if (x11colour == "orchid3")
    return fromRGBA(205, 105, 201);
  if (x11colour == "orchid4")
    return fromRGBA(139, 71, 137);
  if (x11colour == "plum1")
    return fromRGBA(255, 187, 255);
  if (x11colour == "plum2")
    return fromRGBA(238, 174, 238);
  if (x11colour == "plum3")
    return fromRGBA(205, 150, 205);
  if (x11colour == "plum4")
    return fromRGBA(139, 102, 139);
  if (x11colour == "mediumorchid1")
    return fromRGBA(224, 102, 255);
  if (x11colour == "mediumorchid2")
    return fromRGBA(209, 95, 238);
  if (x11colour == "mediumorchid3")
    return fromRGBA(180, 82, 205);
  if (x11colour == "mediumorchid4")
    return fromRGBA(122, 55, 139);
  if (x11colour == "darkorchid1")
    return fromRGBA(191, 62, 255);
  if (x11colour == "darkorchid2")
    return fromRGBA(178, 58, 238);
  if (x11colour == "darkorchid3")
    return fromRGBA(154, 50, 205);
  if (x11colour == "darkorchid4")
    return fromRGBA(104, 34, 139);
  if (x11colour == "purple1")
    return fromRGBA(155, 48, 255);
  if (x11colour == "purple2")
    return fromRGBA(145, 44, 238);
  if (x11colour == "purple3")
    return fromRGBA(125, 38, 205);
  if (x11colour == "purple4")
    return fromRGBA(85, 26, 139);
  if (x11colour == "mediumpurple1")
    return fromRGBA(171, 130, 255);
  if (x11colour == "mediumpurple2")
    return fromRGBA(159, 121, 238);
  if (x11colour == "mediumpurple3")
    return fromRGBA(137, 104, 205);
  if (x11colour == "mediumpurple4")
    return fromRGBA(93, 71, 139);
  if (x11colour == "thistle1")
    return fromRGBA(255, 225, 255);
  if (x11colour == "thistle2")
    return fromRGBA(238, 210, 238);
  if (x11colour == "thistle3")
    return fromRGBA(205, 181, 205);
  if (x11colour == "thistle4")
    return fromRGBA(139, 123, 139);
  if (x11colour == "darkgrey")
    return fromRGBA(169, 169, 169);
  if (x11colour == "darkgray")
    return fromRGBA(169, 169, 169);
  if (x11colour == "darkblue")
    return fromRGBA(0, 0, 139);
  if (x11colour == "darkcyan")
    return fromRGBA(0, 139, 139);
  if (x11colour == "darkmagenta")
    return fromRGBA(139, 0, 139);
  if (x11colour == "darkred")
    return fromRGBA(139, 0, 0);
  if (x11colour == "lightgreen")
    return fromRGBA(144, 238, 144);
  if (x11colour.substr(0, 4) == "grey" ||
      x11colour.substr(0, 4) == "gray")
  {
    std::stringstream ss(x11colour.substr(4, 2));
    float level;
    if (ss >> level)
    {
      level = floor(level*2.55 + 0.5);
      return fromRGBA(level, level, level);
    }
  }

  // default is black
  return fromRGBA(0, 0, 0);
}

void Colour::fromHex(const std::string& hexName)
{
  /* Check to make sure colour is valid */
  if (hexName.at(0) != '#' || hexName.length() < 7)
    abort_program( "Cannot recognise hex colour %s.\n", hexName.c_str());
  for (int i = 1 ; i <= 6 ; i++)
  {
    if (isxdigit(hexName.at(i)) == 0)
      abort_program( "Cannot recognise hex colour %s.\n", hexName.c_str());
  }

  /* Seperate colours */
  std::string red = hexName.substr(1, 2);
  std::string grn = hexName.substr(3, 2);
  std::string blu = hexName.substr(5, 2);

  /* Read colours */
  unsigned int hex;
  sscanf(red.c_str(), "%x", &hex);
  r = hex;
  sscanf(grn.c_str(), "%x", &hex);
  g = hex;
  sscanf(blu.c_str(), "%x", &hex);
  b = hex;
}


