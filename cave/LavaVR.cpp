/********************************************************************************************************************** 
 * LavaVu + OmegaLib
 *********************************************************************************************************************/
#ifdef USE_OMEGALIB

#include <omega.h>
#include <omegaGl.h>
#include <omegaToolkit.h>
#include "../src/ViewerApp.h"
#include "../src/LavaVu.h"
#include "../src/Server.h"

std::vector<std::string> arglist;

using namespace omega;
using namespace omegaToolkit;
using namespace omegaToolkit::ui;

class LavaVuApplication;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class LavaVuRenderPass: public RenderPass, ViewerApp
{
public:
  LavaVuRenderPass(Renderer* client, LavaVuApplication* app, OpenGLViewer* viewer): RenderPass(client, "LavaVuRenderPass"), app(app), ViewerApp(viewer) {}
  virtual void initialize();
  virtual void render(Renderer* client, const DrawContext& context);

   // Virtual functions for interactivity (from ViewerApp/ApplicationInterface)
   virtual bool mouseMove(int x, int y) {}
   virtual bool mousePress(MouseButton btn, bool down, int x, int y) {}
   virtual bool mouseScroll(int scroll) {}
   virtual bool keyPress(unsigned char key, int x, int y) {}

private:
  LavaVuApplication* app;

};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class LavaVuApplication: public EngineModule
{
public:
  OpenGLViewer* viewer;
  LavaVu* glapp;
  bool redisplay;
  bool animate;
  int argc;
  char** argv;
  //Copy of commands
  std::deque<std::string> commands;
  //Widgets
  Ref<Label> statusLabel;

  LavaVuApplication(): EngineModule("LavaVuApplication") { redisplay = true; animate = false; enableSharedData(); }

    virtual void initialize()
    {
        // Initialize the omegaToolkit python API
        omegaToolkitPythonApiInit();

        PythonInterpreter* pi = SystemManager::instance()->getScriptInterpreter();

        // Run the system menu script
        pi->runFile("default_init.py", 0);

        // Call the function from the script that will setup the menu.
        pi->eval("_onAppStart()");

        //Create a label for text info
        DisplaySystem* ds = SystemManager::instance()->getDisplaySystem();
        // Create and initialize the UI management module.
        myUiModule = UiModule::createAndInitialize();
        myUi = myUiModule->getUi();

        statusLabel = Label::create(myUi);
        statusLabel->setText("");
        statusLabel->setColor(Color::Gray);
        int sz = 100;
        statusLabel->setFont(ostr("fonts/arial.ttf %1%", %sz));
        statusLabel->setHorizontalAlign(Label::AlignLeft);
        statusLabel->setPosition(Vector2f(100,100));

    }

  virtual void initializeRenderer(Renderer* r) 
  { 
    viewer = new OpenGLViewer(false, false);
    r->addRenderPass(new LavaVuRenderPass(r, this, viewer));
  }

  float setArgs(int argc, char** argv) {this->argc = argc; this->argv = argv;}

  virtual void handleEvent(const Event& evt);
  virtual void commitSharedData(SharedOStream& out);
  virtual void updateSharedData(SharedIStream& in);

private:
  // The ui manager
  Ref<UiModule> myUiModule;
  // The root ui container
  Ref<Container> myUi;  
  std::string labelText;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LavaVuRenderPass::initialize()
{
  RenderPass::initialize();

  //Init fractal app
  //Fake the arg list from vector of args
  int argc = arglist.size()+1;
  char* argv[argc];
  argv[0] = (char*)malloc(20);
  strcpy(argv[0], "./gLucifer");
  for (int i=1; i<argc; i++)
  {
    argv[i] = (char*)arglist[i-1].c_str();
    std::cerr << argv[i] << "\n";
  }

   //Add any output attachments to the viewer
#ifndef DISABLE_SERVER
   SystemManager* sys = SystemManager::instance();
   if (sys->isMaster())
   {
      std::string htmlpath = std::string("./html");
      //Quality = 0, don't serve images
      viewer->addOutput(Server::Instance(viewer, htmlpath, 8080, 0, 4));
   }
#endif

  //Create the app
  app->glapp = new LavaVu(arglist, viewer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LavaVuRenderPass::render(Renderer* client, const DrawContext& context)
{
  if(context.task == DrawContext::SceneDrawTask)
  {
    client->getRenderer()->beginDraw3D(context);
    Camera* cam = Engine::instance()->getDefaultCamera(); // equivalent to the getDefaultCamera python call.

    if (!viewer->isopen)
    {
      //Load vis data for first window
      if (!app->glapp->loadWindow(0, -1, true))
         abort_program("Model file load error, no window data\n");

      DisplaySystem* ds = app->getEngine()->getDisplaySystem();
      Colour& bg = viewer->background;
      ds->setBackgroundColor(Color(bg.rgba[0]/255.0, bg.rgba[1]/255.0, bg.rgba[2]/255.0, 0));
      //Omegalib 5.1+
      //cam->setBackgroundColor(Color(bg.rgba[0]/255.0, bg.rgba[1]/255.0, bg.rgba[2]/255.0, 0));

      viewer->open(context.tile->pixelSize[0], context.tile->pixelSize[1]);
      viewer->init();

      //Setup camera using omegalib functions
      CameraController* cc = cam->getController();

      cam->setEyeSeparation(0.03); //TEST: reducing eye separation

         View* view = app->glapp->aview;
         
         float rotate[4], translate[3], focus[3];
         view->getCamera(rotate, translate, focus);

        //At viewing distance
        cam->setPosition(Vector3f(focus[0], focus[1], (focus[2] - view->model_size)) * view->orientation);
        //At center
        //cam->setPosition(Vector3f(focus[0], focus[1], focus[2] * view->orientation));

        cam->lookAt(Vector3f(focus[0], focus[1], focus[2] * view->orientation), Vector3f(0,1,0));

        //cam->setPitchYawRoll(Vector3f(0.0, 0.0, 0.0));
        //cam->setNearFarZ(view->near_clip*0.01, view->far_clip);
        //NOTE: Setting near clip too close is bad for eyes, too far kills negative parallax stereo
        cam->setNearFarZ(view->near_clip*0.1, view->far_clip);
        //cam->setNearFarZ(cam->getNearZ(), view->far_clip);
         cc->setSpeed(view->model_size * 0.1);
         
    }

    //Copy commands before consumed
    app->commands = OpenGLViewer::commands;
    //Fade out status label
    app->statusLabel->setAlpha(app->statusLabel->getAlpha() * 0.995);
     
    //if (app->redisplay)
    {
      glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

       glMatrixMode(GL_MODELVIEW);
         View* view = app->glapp->aview;
         if (view->scale[0] != 1.0 || view->scale[1] != 1.0 || view->scale[2] != 1.0)
         {
            glScalef(view->scale[0], view->scale[1], view->scale[2] * view->orientation);
            // Enable automatic rescaling of normal vectors when scaling is turned on
            //glEnable(GL_RESCALE_NORMAL);
            glEnable(GL_NORMALIZE);
            GL_Error_Check;
         }
         
      viewer->display();
      app->redisplay = false;
      view->rotated = false;
    }

    client->getRenderer()->endDraw();
  }
  //else if(context.task == DrawContext::OverlayDrawTask)
  //{
  //}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LavaVuApplication::handleEvent(const Event& evt)
{
  //printf(". %d %d %d\n", evt.getType(), evt.getServiceType(), evt.getFlags());
  if(evt.getServiceType() == Service::Pointer)
  {
    int x = evt.getPosition().x();
    int y = evt.getPosition().y();
    int flags = evt.getFlags();
    MouseButton button = NoButton;
    if (flags & 1)
      button = LeftButton;
    else if (flags & 2)
      button = RightButton;
    else if (flags & 4)
      button = MiddleButton;

    switch (evt.getType())
    {
      case Event::Down:
        //printf("%d %d\n", button, flags);
        if (button <= RightButton) viewer->mouseState ^= (int)pow(2, (int)button);
        viewer->mousePress(button, true, x, y);
          redisplay = true;
          break;
      case Event::Up:
        viewer->mouseState = 0;
        viewer->mousePress(button, false, x, y);
          break;
      case Event::Zoom:
        viewer->mouseScroll(evt.getExtraDataInt(0));
         break;
      case Event::Move:
         if (viewer->mouseState)
         {
            viewer->mouseMove(x, y);
            //redisplay = true;
         }
          break;
      default:
        printf("? %d\n", evt.getType());
    }
  }
  else if(evt.getServiceType() == Service::Keyboard)
  {
    int x = evt.getPosition().x();
    int y = evt.getPosition().y();
    int key = evt.getSourceId();
    if (evt.isKeyDown(key))
    {
      if (key > 255)
      {
      //printf("Key %d %d\n", key, evt.getFlags());
        if (key == 262) key = KEY_UP;
        else if (key == 264) key = KEY_DOWN;
        else if (key == 261) key = KEY_LEFT;
        else if (key == 263) key = KEY_RIGHT;
        else if (key == 265) key = KEY_PAGEUP;
        else if (key == 266) key = KEY_PAGEDOWN;
        else if (key == 260) key = KEY_HOME;
        else if (key == 267) key = KEY_END;
      }
      //viewer->keyPress(key, x, y);
    }
  }
  else if(evt.getServiceType() == Service::Wand)
  {
    int x = evt.getPosition().x();
    int y = evt.getPosition().y();
    int key = evt.getSourceId();
    if (evt.isButtonDown(Event::Button2)) //Circle
    {
    }
    if (evt.isButtonDown(Event::Button3)) //Cross
    {
      //Change point type
      glapp->parseCommands("pointtype all");
    }
    else if (evt.isButtonDown(Event::Button5))
    {
      //printf("Key %d %d\n", key, evt.getFlags());
     //viewer->keyPress(key, x, y);

      //glapp->parseCommands("sort");
            printf("Wand  Analog trigger L1");
      glapp->aview->rotated = true;

    }
    else if (evt.isButtonDown(Event::ButtonUp ))
    {
        printf("Wand D-Pad up pressed\n");
        //glapp->parseCommands("stop");
        animate = false;
        //if (GeomData::opacity > 0.0) GeomData::opacity -= 0.05;
        //glapp->redrawViewports();
    }
    else if (evt.isButtonDown(Event::ButtonDown ))
    {
        printf("Wand D-Pad down pressed\n");
        //glapp->parseCommands("play");
        animate = true;
        //if (GeomData::opacity < 1.0) GeomData::opacity += 0.05;
        //glapp->redrawViewports();
    }
    else if (evt.isButtonDown(Event::ButtonLeft ))
    {
        //These work when analogue stick disabled
        printf("Wand D-Pad left pressed\n");
        //glapp->parseCommands("timestep up");
        glapp->parseCommands("scale points down");
    }
    else if (evt.isButtonDown( Event::ButtonRight ))
    {
        //These work when analogue stick disabled
        printf("Wand D-Pad right pressed\n");
        //glapp->parseCommands("next");
        glapp->parseCommands("scale points up");
    }
    else
    {
        //Grab the analog stick horizontal axis
        float analogLR = evt.getAxis(0);
        //Grab the analog stick vertical axis
        float analogUD = evt.getAxis(1);

        if (abs(analogUD) + abs(analogLR) > 0.001)
        {
            if (abs(analogUD) > abs(analogLR))
            {
               if (analogUD > 0.01)
                 glapp->parseCommands("timestep down");
               else if (analogUD < 0.01)
                 glapp->parseCommands("timestep up");
               evt.setProcessed();
            }
        }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LavaVuApplication::commitSharedData(SharedOStream& out)
{
   std::stringstream oss;
   for (int i=0; i < commands.size(); i++)
      oss << commands[i] << std::endl;
   out << oss.str();
   out << glapp->message;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LavaVuApplication::updateSharedData(SharedIStream& in)
{
   std::string commandstr;
   in >> commandstr;
   in >> glapp->message;

   SystemManager* sys = SystemManager::instance();
   if (!sys->isMaster())
   {
    OpenGLViewer::commands.clear();
    std::stringstream iss(commandstr);
    std::string line;
    while(std::getline(iss, line))
    {
        OpenGLViewer::commands.push_back(line);
        //glapp->parseCommands(line);
    }
   }

   if (animate) glapp->parseCommands("next");

   //Update status label
   if (statusLabel->getText() != glapp->message)
   {
      statusLabel->setAlpha(1.0);
      statusLabel->setText(glapp->message);
   }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ApplicationBase entry point
int main(int argc, char** argv)
{
  Application<LavaVuApplication> app("gLucifer");
  oargs().setStringVector("gLucifer", "gLucifer Arguments", arglist);
  return omain(app, argc, argv);
}

#endif
