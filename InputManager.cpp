/*
InputManager from OGRE engine.
With a bit of changes.
*/

#include <sstream>

#include <InputManager.h>

namespace SE {

//InputManager *InputManager::mInputManager;

InputManager::InputManager() :
pMouse( 0 ),
pKeyboard( 0 ),
pInputSystem( 0 ) {

}

InputManager::InputManager( const InputManager& ) { ;; }

InputManager::~InputManager() throw() {

if( pInputSystem ) {
  if( pMouse ) {
    pInputSystem->destroyInputObject( pMouse );
    pMouse = 0;
  }

  if( pKeyboard ) {
    pInputSystem->destroyInputObject( pKeyboard );
    pKeyboard = 0;
  }

  if( mJoysticks.size() > 0 ) {
    itJoystick    = mJoysticks.begin();
    itJoystickEnd = mJoysticks.end();
    for(; itJoystick != itJoystickEnd; ++itJoystick ) {
      pInputSystem->destroyInputObject( *itJoystick );
    }

    mJoysticks.clear();
  }

  // If you use OIS1.0RC1 or above, uncomment this line
  // and comment the line below it
  pInputSystem->destroyInputSystem( pInputSystem );
  //pInputSystem->destroyInputSystem();
  pInputSystem = 0;

  // Clear Listeners
  oKeyListeners.clear();
  oMouseListeners.clear();
  oJoystickListeners.clear();
}
}

void InputManager::Initialise(const uint32_t window_id, const int32_t width, const int32_t height) {

  if( !pInputSystem ) {
    // Setup basic variables
    OIS::ParamList paramList;    
    std::ostringstream window_hnd_str;

    // Fill parameter list
    window_hnd_str << window_id;
    paramList.insert( std::make_pair( std::string( "WINDOW" ), window_hnd_str.str() ) );

    // Create inputsystem
    pInputSystem = OIS::InputManager::createInputSystem( paramList );

    // If possible create a buffered keyboard
    // (note: if below line doesn't compile, try:  if (pInputSystem->getNumberOfDevices(OIS::OISKeyboard) > 0) 
    //if( pInputSystem->nupKeyboards() > 0 ) 
    if (pInputSystem->getNumberOfDevices(OIS::OISKeyboard) > 0) {
      pKeyboard = static_cast<OIS::Keyboard*>( pInputSystem->createInputObject( OIS::OISKeyboard, true ) );
      if (!pKeyboard) {
        fprintf(stderr, "can't find Keyboard\n");
        exit(-1);
      }
      pKeyboard->setEventCallback( this );
    }
    else {
      fprintf(stderr, "can't find Keyboard\n");
      exit(-1);
    }

    // If possible create a buffered mouse
    // (note: if below line doesn't compile, try:  if (pInputSystem->getNumberOfDevices(OIS::OISMouse) > 0) 
    //if( pInputSystem->numMice() > 0 ) 
    if (pInputSystem->getNumberOfDevices(OIS::OISMouse) > 0) {
      pMouse = static_cast<OIS::Mouse*>( pInputSystem->createInputObject( OIS::OISMouse, true ) );
      if (!pMouse) {
        fprintf(stderr, "can't find Mouse\n");
        exit(-1);
      }
      pMouse->setEventCallback( this );

      // Set mouse region
      SetWindowExtents(width, height);
    }
    else {
      fprintf(stderr, "can't find Mouse\n");
      exit(-1);
    }

    // If possible create all joysticks in buffered mode
    // (note: if below line doesn't compile, try:  if (pInputSystem->getNumberOfDevices(OIS::OISJoyStick) > 0) 
    //if( pInputSystem->numJoySticks() > 0 ) 
    if (pInputSystem->getNumberOfDevices(OIS::OISJoyStick) > 0) {
      //mJoysticks.resize( pInputSystem->numJoySticks() );
      mJoysticks.resize( pInputSystem->getNumberOfDevices(OIS::OISJoyStick) );

      itJoystick    = mJoysticks.begin();
      itJoystickEnd = mJoysticks.end();
      for(; itJoystick != itJoystickEnd; ++itJoystick ) {
        (*itJoystick) = static_cast<OIS::JoyStick*>( pInputSystem->createInputObject( OIS::OISJoyStick, true ) );
        (*itJoystick)->setEventCallback( this );
      }
    }
  }
}

void InputManager::Capture() {

  // Need to capture / update each device every frame
  if( pMouse ) {
    pMouse->capture();
  }

  if( pKeyboard ) {
    pKeyboard->capture();
  }

  if( mJoysticks.size() > 0 ) {
    itJoystick    = mJoysticks.begin();
    itJoystickEnd = mJoysticks.end();
    for(; itJoystick != itJoystickEnd; ++itJoystick ) {
      (*itJoystick)->capture();
    }
  }
}

void InputManager::AddKeyListener( OIS::KeyListener *keyListener, const std::string& instanceName ) {
  if( pKeyboard ) {
    // Check for duplicate items
    itKeyListener = oKeyListeners.find( instanceName );
    if( itKeyListener == oKeyListeners.end() ) {
      oKeyListeners[ instanceName ] = keyListener;
    }
    else {
      // Duplicate Item
    }
  }
}

void InputManager::AddMouseListener( OIS::MouseListener *mouseListener, const std::string& instanceName ) {
  if( pMouse ) {
    // Check for duplicate items
    itMouseListener = oMouseListeners.find( instanceName );
    if( itMouseListener == oMouseListeners.end() ) {
      oMouseListeners[ instanceName ] = mouseListener;
    }
    else {
      // Duplicate Item
    }
  }
}

void InputManager::AddJoystickListener( OIS::JoyStickListener *joystickListener, const std::string& instanceName ) {
  if( mJoysticks.size() > 0 ) {
    // Check for duplicate items
    itJoystickListener = oJoystickListeners.find( instanceName );
    if( itJoystickListener == oJoystickListeners.end() ) {
      oJoystickListeners[ instanceName ] = joystickListener;
    }
    else {
      // Duplicate Item
    }
  }
}

void InputManager::RemoveKeyListener( const std::string& instanceName ) {
  // Check if item exists
  itKeyListener = oKeyListeners.find( instanceName );
  if( itKeyListener != oKeyListeners.end() ) {
    oKeyListeners.erase( itKeyListener );
  }
  else {
    // Doesn't Exist
  }
}

void InputManager::RemoveMouseListener( const std::string& instanceName ) {
  // Check if item exists
  itMouseListener = oMouseListeners.find( instanceName );
  if( itMouseListener != oMouseListeners.end() ) {
    oMouseListeners.erase( itMouseListener );
  }
  else {
    // Doesn't Exist
  }
}

void InputManager::RemoveJoystickListener( const std::string& instanceName ) {
  // Check if item exists
  itJoystickListener = oJoystickListeners.find( instanceName );
  if( itJoystickListener != oJoystickListeners.end() ) {
    oJoystickListeners.erase( itJoystickListener );
  }
  else {
    // Doesn't Exist
  }
}

void InputManager::RemoveKeyListener( OIS::KeyListener *keyListener ) {
  itKeyListener    = oKeyListeners.begin();
  itKeyListenerEnd = oKeyListeners.end();
  for(; itKeyListener != itKeyListenerEnd; ++itKeyListener ) {
    if( itKeyListener->second == keyListener ) {
      oKeyListeners.erase( itKeyListener );
      break;
    }
  }
}

void InputManager::RemoveMouseListener( OIS::MouseListener *mouseListener ) {
  itMouseListener    = oMouseListeners.begin();
  itMouseListenerEnd = oMouseListeners.end();
  for(; itMouseListener != itMouseListenerEnd; ++itMouseListener ) {
    if( itMouseListener->second == mouseListener ) {
      oMouseListeners.erase( itMouseListener );
      break;
    }
  }
}

void InputManager::RemoveJoystickListener( OIS::JoyStickListener *joystickListener ) {
  itJoystickListener    = oJoystickListeners.begin();
  itJoystickListenerEnd = oJoystickListeners.end();
  for(; itJoystickListener != itJoystickListenerEnd; ++itJoystickListener ) {
    if( itJoystickListener->second == joystickListener ) {
      oJoystickListeners.erase( itJoystickListener );
      break;
    }
  }
}

void InputManager::RemoveAllListeners() {
  oKeyListeners.clear();
  oMouseListeners.clear();
  oJoystickListeners.clear();
}

void InputManager::RemoveAllKeyListeners() {
  oKeyListeners.clear();
}

void InputManager::RemoveAllMouseListeners() {
  oMouseListeners.clear();
}

void InputManager::RemoveAllJoystickListeners() {
  oJoystickListeners.clear();
}

void InputManager::SetWindowExtents(const int32_t width, const int32_t height ) {

  if (!pMouse) return;
  
  // Set mouse region (if window resizes, we should alter this to reflect as well)
  const OIS::MouseState &mouseState = pMouse->getMouseState();
  mouseState.width  = width;
  mouseState.height = height;
}

OIS::Mouse* InputManager::GetMouse() const {
  return pMouse;
}

OIS::Keyboard* InputManager::GetKeyboard() const {
  return pKeyboard;
}

OIS::JoyStick* InputManager::GetJoystick( unsigned int index ) const{
  // Make sure it's a valid index
  if( index < mJoysticks.size() ) {
    return mJoysticks[ index ];
  }

  return 0;
}

size_t InputManager::GetNumOfJoysticks() const {
  // Cast to keep compiler happy ^^
  return mJoysticks.size();
}

bool InputManager::keyPressed( const OIS::KeyEvent &e ) {
  itKeyListener    = oKeyListeners.begin();
  itKeyListenerEnd = oKeyListeners.end();
  for(; itKeyListener != itKeyListenerEnd; ++itKeyListener ) {
    if(!itKeyListener->second->keyPressed( e ))
      break;
  }

  return true;
}

bool InputManager::keyReleased( const OIS::KeyEvent &e ) {
  itKeyListener    = oKeyListeners.begin();
  itKeyListenerEnd = oKeyListeners.end();
  for(; itKeyListener != itKeyListenerEnd; ++itKeyListener ) {
    if(!itKeyListener->second->keyReleased( e ))
      break;
  }

  return true;
}

bool InputManager::mouseMoved( const OIS::MouseEvent &e ) {
  itMouseListener    = oMouseListeners.begin();
  itMouseListenerEnd = oMouseListeners.end();
  for(; itMouseListener != itMouseListenerEnd; ++itMouseListener ) {
    if(!itMouseListener->second->mouseMoved( e ))
      break;
  }

  return true;
}

bool InputManager::mousePressed( const OIS::MouseEvent &e, OIS::MouseButtonID id ) {
  itMouseListener    = oMouseListeners.begin();
  itMouseListenerEnd = oMouseListeners.end();
  for(; itMouseListener != itMouseListenerEnd; ++itMouseListener ) {
    if(!itMouseListener->second->mousePressed( e, id ))
      break;
  }

  return true;
}

bool InputManager::mouseReleased( const OIS::MouseEvent &e, OIS::MouseButtonID id ) {
  itMouseListener    = oMouseListeners.begin();
  itMouseListenerEnd = oMouseListeners.end();
  for(; itMouseListener != itMouseListenerEnd; ++itMouseListener ) {
    if(!itMouseListener->second->mouseReleased( e, id ))
      break;
  }

  return true;
}

bool InputManager::povMoved( const OIS::JoyStickEvent &e, int pov ) {
  itJoystickListener    = oJoystickListeners.begin();
  itJoystickListenerEnd = oJoystickListeners.end();
  for(; itJoystickListener != itJoystickListenerEnd; ++itJoystickListener ) {
    if(!itJoystickListener->second->povMoved( e, pov ))
      break;
  }

  return true;
}

bool InputManager::axisMoved( const OIS::JoyStickEvent &e, int axis ) {
  itJoystickListener    = oJoystickListeners.begin();
  itJoystickListenerEnd = oJoystickListeners.end();
  for(; itJoystickListener != itJoystickListenerEnd; ++itJoystickListener ) {
    if(!itJoystickListener->second->axisMoved( e, axis ))
      break;
  }

  return true;
}

bool InputManager::sliderMoved( const OIS::JoyStickEvent &e, int sliderID ) {
  itJoystickListener    = oJoystickListeners.begin();
  itJoystickListenerEnd = oJoystickListeners.end();
  for(; itJoystickListener != itJoystickListenerEnd; ++itJoystickListener ) {
    if(!itJoystickListener->second->sliderMoved( e, sliderID ))
      break;
  }

  return true;
}

bool InputManager::buttonPressed( const OIS::JoyStickEvent &e, int button ) {
  itJoystickListener    = oJoystickListeners.begin();
  itJoystickListenerEnd = oJoystickListeners.end();
  for(; itJoystickListener != itJoystickListenerEnd; ++itJoystickListener ) {
    if(!itJoystickListener->second->buttonPressed( e, button ))
      break;
  }

  return true;
}

bool InputManager::buttonReleased( const OIS::JoyStickEvent &e, int button ) {
  itJoystickListener    = oJoystickListeners.begin();
  itJoystickListenerEnd = oJoystickListeners.end();
  for(; itJoystickListener != itJoystickListenerEnd; ++itJoystickListener ) {
    if(!itJoystickListener->second->buttonReleased( e, button ))
      break;
  }

  return true;
}

/*
InputManager* InputManager::getSingletonPtr() {
  if( !mInputManager ) {
    mInputManager = new InputManager();
  }

  return mInputManager;
}
*/

} //namespace SE


