/*
  InputManager from OGRE engine.
  With a bit of changes.
*/


#ifndef __INPUT_MANAGER_H__
#define __INPUT_MANAGER_H__ 1

#include <OISMouse.h>
#include <OISKeyboard.h>
#include <OISJoyStick.h>
#include <OISInputManager.h>

#include <stdint.h>

namespace SE {

class InputManager : public OIS::KeyListener, public OIS::MouseListener, public OIS::JoyStickListener {

  InputManager( const InputManager& ); 
  InputManager & operator = ( const InputManager& );

  bool keyPressed( const OIS::KeyEvent &e );
  bool keyReleased( const OIS::KeyEvent &e );

  bool mouseMoved( const OIS::MouseEvent &e );
  bool mousePressed( const OIS::MouseEvent &e, OIS::MouseButtonID id );
  bool mouseReleased( const OIS::MouseEvent &e, OIS::MouseButtonID id );

  bool povMoved( const OIS::JoyStickEvent &e, int pov );
  bool axisMoved( const OIS::JoyStickEvent &e, int axis );
  bool sliderMoved( const OIS::JoyStickEvent &e, int sliderID );
  bool buttonPressed( const OIS::JoyStickEvent &e, int button );
  bool buttonReleased( const OIS::JoyStickEvent &e, int button );

  OIS::Mouse        *pMouse;
  OIS::Keyboard     *pKeyboard;
  OIS::InputManager *pInputSystem;

  std::vector<OIS::JoyStick*> mJoysticks;
  std::vector<OIS::JoyStick*>::iterator itJoystick;
  std::vector<OIS::JoyStick*>::iterator itJoystickEnd;

  std::map<std::string, OIS::KeyListener*> oKeyListeners;
  std::map<std::string, OIS::MouseListener*> oMouseListeners;
  std::map<std::string, OIS::JoyStickListener*> oJoystickListeners;

  std::map<std::string, OIS::KeyListener*>::iterator itKeyListener;
  std::map<std::string, OIS::MouseListener*>::iterator itMouseListener;
  std::map<std::string, OIS::JoyStickListener*>::iterator itJoystickListener;

  std::map<std::string, OIS::KeyListener*>::iterator itKeyListenerEnd;
  std::map<std::string, OIS::MouseListener*>::iterator itMouseListenerEnd;
  std::map<std::string, OIS::JoyStickListener*>::iterator itJoystickListenerEnd;

  //static InputManager *mInputManager;

  public:
    
  InputManager();
  ~InputManager() throw();

  void Initialise(const uint32_t window_id, const int32_t width, const int32_t height);
  void Capture();

  void AddKeyListener( OIS::KeyListener *keyListener, const std::string& instanceName );
  void AddMouseListener( OIS::MouseListener *mouseListener, const std::string& instanceName );
  void AddJoystickListener( OIS::JoyStickListener *joystickListener, const std::string& instanceName );

  void RemoveKeyListener( const std::string& instanceName );
  void RemoveMouseListener( const std::string& instanceName );
  void RemoveJoystickListener( const std::string& instanceName );

  void RemoveKeyListener( OIS::KeyListener *keyListener );
  void RemoveMouseListener( OIS::MouseListener *mouseListener );
  void RemoveJoystickListener( OIS::JoyStickListener *joystickListener );

  void RemoveAllListeners();
  void RemoveAllKeyListeners();
  void RemoveAllMouseListeners();
  void RemoveAllJoystickListeners();

  void SetWindowExtents(const int32_t width, const int32_t height);

  OIS::Mouse*    GetMouse();
  OIS::Keyboard* GetKeyboard();
  OIS::JoyStick* GetJoystick( unsigned int index );

  int getNumOfJoysticks();

  //static InputManager* getSingletonPtr();
};

} // namespace SE

#endif

