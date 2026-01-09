#pragma once

#include <Arduino.h>
#include <Adafruit_Protomatter.h>
#include <FlashStorage.h>

#include "Scene.h"
#include "scenes/FlowFieldScene.h"
#include "scenes/ReactionDiffusionScene.h"

class SceneManager {
public:
  SceneManager(Adafruit_Protomatter &matrix, uint8_t button_pin);
  
  void begin();
  void tick(uint32_t now_ms);
  void update(uint32_t dt_ms);
  void render();
  void setWeather(const WeatherParams &params);
  
  Scene* getActiveScene() const { return active_scene_; }

  // Persistent storage structure
  struct PersistentState {
    uint32_t magic;
    uint8_t current_scene_id;
    uint8_t reserved[3];
  };

private:
  static constexpr uint32_t kMagic = 0xA55A0001;
  static constexpr uint8_t kSceneCount = 2;
  static constexpr uint32_t kDebounceMs = 50;

  Adafruit_Protomatter &matrix_;
  uint8_t button_pin_;
  
  PersistentState state_;

  // Scenes
  FlowFieldScene flowFieldScene_;
  ReactionDiffusionScene reactionDiffusionScene_;
  Scene *active_scene_;

  // Button state
  bool last_button_state_;
  uint32_t last_debounce_time_;
  bool button_pressed_event_;

  void loadState();
  void saveState();
  void switchScene(uint8_t scene_id);
};
