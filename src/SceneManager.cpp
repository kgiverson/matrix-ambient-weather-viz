#include "SceneManager.h"

// Define the flash storage slot (must be global/file scope)
FlashStorage(flash_store, SceneManager::PersistentState);

SceneManager::SceneManager(Adafruit_Protomatter &matrix, uint8_t button_pin)
    : matrix_(matrix), button_pin_(button_pin), active_scene_(nullptr),
      internal_scene_index_(0),
      last_button_state_(HIGH), last_debounce_time_(0),
      button_pressed_event_(false) {}

void SceneManager::begin() {
  pinMode(button_pin_, INPUT_PULLUP);
  last_button_state_ = digitalRead(button_pin_);
  
  loadState();
  
  // Validate loaded ID
  if (state_.current_scene_id >= kSceneCount) {
    state_.current_scene_id = 0;
  }

  switchScene(state_.current_scene_id);
}

void SceneManager::tick(uint32_t now_ms) {
  // Button Debounce logic
  bool reading = digitalRead(button_pin_);

  if (reading != last_button_state_) {
    last_debounce_time_ = now_ms;
  }

  if ((now_ms - last_debounce_time_) > kDebounceMs) {
    // State has been stable
    // Check for falling edge (Button Press if PULLUP)
    // We want to trigger when it BECOMES LOW (pressed)
    // Assuming active-low
    static bool stable_state = HIGH;
    
    if (reading != stable_state) {
      stable_state = reading;
      if (stable_state == LOW) {
        // Pressed
        uint8_t next_id = (state_.current_scene_id + 1) % kSceneCount;
        switchScene(next_id);
        
        // Save new preference
        state_.current_scene_id = next_id;
        saveState();
      }
    }
  }

  last_button_state_ = reading;
}

void SceneManager::update(uint32_t dt_ms) {
  if (active_scene_) {
    active_scene_->update(dt_ms);
  }
}

void SceneManager::render() {
  if (active_scene_) {
    active_scene_->render(matrix_);
  }
}

void SceneManager::setWeather(const WeatherParams &params) {
  // Propagate to all scenes (so they are ready when switched to)
  flowFieldScene_.setWeather(params);
  reactionDiffusionScene_.setWeather(params);
  curlNoiseScene_.setWeather(params);
}

void SceneManager::switchScene(uint8_t scene_id) {
  matrix_.fillScreen(0);
  
  uint8_t target_id = scene_id;
  if (scene_id == kCycleModeId) {
    target_id = internal_scene_index_;
  }

  if (target_id == 0) {
    active_scene_ = &flowFieldScene_;
  } else if (target_id == 1) {
    active_scene_ = &reactionDiffusionScene_;
  } else if (target_id == 2) {
    active_scene_ = &curlNoiseScene_;
  } else {
    active_scene_ = &flowFieldScene_; // Default
  }
  
  state_.current_scene_id = scene_id;
  active_scene_->begin(matrix_);
}

void SceneManager::cycleSceneIfEnabled() {
  if (state_.current_scene_id == kCycleModeId) {
    internal_scene_index_ = (internal_scene_index_ + 1) % 3; // Cycle through 3 real scenes
    switchScene(kCycleModeId);
    Serial.print("SceneManager: cycling to internal index ");
    Serial.println(internal_scene_index_);
  }
}

void SceneManager::loadState() {
  state_ = flash_store.read();
  if (state_.magic != kMagic) {
    // First run or invalid data
    state_.magic = kMagic;
    state_.current_scene_id = 0;
    // We don't necessarily need to write back immediately, 
    // waiting for first interaction saves a write cycle.
    Serial.println("SceneManager: storage invalid, defaulting to 0");
  } else {
    Serial.print("SceneManager: loaded scene ");
    Serial.println(state_.current_scene_id);
  }
}

void SceneManager::saveState() {
  state_.magic = kMagic;
  flash_store.write(state_);
  Serial.print("SceneManager: saved scene ");
  Serial.println(state_.current_scene_id);
}
