/********************************************************************************************************************** 
 * Attempt to get gLucifer running in OmegaLib
 *********************************************************************************************************************/
#ifdef USE_OMEGALIB

#include <omega.h>
#include <omegaGl.h>
#include <omegaToolkit.h>
#include "src/ViewerApp.h"
#include "src/GLuciferViewer.h"
#include "src/GLuciferServer.h"

std::vector<std::string> args;

using namespace omega;

class GLuciferApplication;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GLuciferRenderPass: public RenderPass, ViewerApp
{
public:
  GLuciferRenderPass(Renderer* client, GLuciferApplication* app, OpenGLViewer* viewer): RenderPass(client, "GLuciferRenderPass"), app(app), ViewerApp(viewer) {}
  virtual void initialize();
  virtual void render(Renderer* client, const DrawContext& context);

   // Virtual functions for interactivity (from ViewerApp/ApplicationInterface)
   virtual bool mouseMove(int x, int y) {}
   virtual bool mousePress(MouseButton btn, bool down, int x, int y) {}
   virtual bool mouseScroll(int scroll) {}
   virtual bool keyPress(unsigned char key, int x, int y) {}

private:
  GLuciferApplication* app;

};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class GLuciferApplication: public EngineModule
{
public:
  OpenGLViewer* viewer;
  GLuciferViewer* glapp;
  bool redisplay = true;
  bool animate = false;
  int argc;
  char** argv;

  GLuciferApplication(): EngineModule("GLuciferApplication") { enableSharedData(); }

    virtual void initialize()
    {
        // Initialize the omegaToolkit python API
        omegaToolkitPythonApiInit();

        PythonInterpreter* pi = SystemManager::instance()->getScriptInterpreter();

        // Run the system menu script
        pi->runFile("default_init.py", 0);

        // Call the function from the script that will setup the menu.
        pi->eval("_onAppStart()");
    }

  virtual void initializeRenderer(Renderer* r) 
  { 
    viewer = new OpenGLViewer(false, false);
    r->addRenderPass(new GLuciferRenderPass(r, this, viewer));
  }

  float setArgs(int argc, char** argv) {this->argc = argc; this->argv = argv;}

  float getYaw() { return myYaw; }
  float getPitch() { return myPitch; }

  virtual void handleEvent(const Event& evt);
  virtual void commitSharedData(SharedOStream& out);
  virtual void updateSharedData(SharedIStream& in);

private:
  float myYaw;
  float myPitch;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GLuciferRenderPass::initialize()
{
  RenderPass::initialize();

  //Init fractal app
    DisplaySystem* ds = app->getEngine()->getDisplaySystem();
    Vector2i resolution = ds->getCanvasSize();
  //Fake the arg list from vector of args
  int argc = args.size()+1;
  char* argv[argc];
  argv[0] = (char*)malloc(20);
  strcpy(argv[0], "./gLucifer");
  for (int i=1; i<argc; i++)
    argv[i] = (char*)args[i-1].c_str();

   //Add any output attachments to the viewer
#ifndef DISABLE_SERVER
   //Not currently working with omegalib...
   std::string htmlpath = std::string("src/html");
   //viewer->addOutput(GLuciferServer::Instance(viewer, htmlpath, 8080, 90, 1));
#endif

  //Create the app
  app->glapp = new GLuciferViewer(args, viewer, resolution[0], resolution[1]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GLuciferRenderPass::render(Renderer* client, const DrawContext& context)
{
  if(context.task == DrawContext::SceneDrawTask)
  {
    client->getRenderer()->beginDraw3D(context);

    if (!viewer->isopen)
    {
      //Load vis data for first window
      if (!app->glapp->loadWindow(0, -1, true))
         abort_program("Model file load error, no window data\n");

      DisplaySystem* ds = app->getEngine()->getDisplaySystem();
      Vector2i resolution = ds->getCanvasSize();
      viewer->open(resolution[0], resolution[1]);
      Colour& bg = viewer->background;
      ds->setBackgroundColor(Color(bg.rgba[0]/255.0, bg.rgba[1]/255.0, bg.rgba[2]/255.0, 0));

      //Setup camera using omegalib functions
      Camera* cam = Engine::instance()->getDefaultCamera(); // equivalent to the getDefaultCamera python call.
      CameraController* cc = cam->getController();

      //cam->setEyeSeparation(0.03); //TEST: setting eye separation

         View* view = app->glapp->aview;
         float rotate[4], translate[3], focus[3];
         view->getCamera(rotate, translate, focus);

        //At viewing distance
        cam->setPosition(Vector3f(focus[0], focus[1], (focus[2] - view->model_size)) * view->orientation);
        //At center
        //cam->setPosition(Vector3f(focus[0], focus[1], focus[2] * view->orientation));

        cam->lookAt(Vector3f(focus[0], focus[1], focus[2] * view->orientation), Vector3f(0,1,0));

        //cam->setPitchYawRoll(Vector3f(0.0, 0.0, 0.0));
        cam->setNearFarZ(view->near_clip*0.01, view->far_clip);
        //cam->setNearFarZ(cam->getNearZ(), view->far_clip);
         cc->setSpeed(view->model_size * 0.1);
      viewer->display();

    }
    //if (app->redisplay)
    {
   glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

       glMatrixMode(GL_MODELVIEW);
         //View* view = app->glapp->aview;
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
      //printf("display\n");
      view->rotated = false;
    }

    client->getRenderer()->endDraw();
  }
  //else if(context.task == DrawContext::OverlayDrawTask)
  //{
  //}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GLuciferApplication::handleEvent(const Event& evt)
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



    /*/ Normalize the mouse position using the total display resolution, 
    // then multiply to get 180 degree rotations
    DisplaySystem* ds = getEngine()->getDisplaySystem();
    Vector2i resolution = ds->getCanvasSize();
    myYaw = (evt.getPosition().x() / resolution[0]) * 180;
    myPitch = (evt.getPosition().y() / resolution[1]) * 180;
    */
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
      viewer->keyPress(key, x, y);
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
void GLuciferApplication::commitSharedData(SharedOStream& out)
{
  out << myYaw << myPitch;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GLuciferApplication::updateSharedData(SharedIStream& in)
{
   in >> myYaw >> myPitch;
   if (animate) glapp->parseCommands("next");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ApplicationBase entry point
int main(int argc, char** argv)
{
  Application<GLuciferApplication> app("gLucifer");
  oargs().setStringVector("gLucifer", "gLucifer Arguments", args);
  return omain(app, argc, argv);
}

#endif
