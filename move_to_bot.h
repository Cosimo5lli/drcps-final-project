// for compiling for real kilobots
#if !defined(NULL)
#define NULL ((void *)0)
#endif

// good values are 3 or 4
// 2 leads to a spiral diverging too slowly
// values bigger than 5 leads to bots dispersing too much
#define SPIRAL_INCREMENT 3
// time in seconds after which the spiral movent of the bot will be reset
#define SECONDS_RESET_SPIRAL 65
// 2 seconds, after that time the bot should consider to be oout of
// communication range and start to move to search for other bots
#define TIME_TO_CONSIDER_OUT_OF_RANGE 40
// the threashold for considering 2 bots touching
#define RANGE_TO_TOUCH 50

// declare motion variable type
typedef enum { STOP, FORWARD, LEFT, RIGHT } motion_t;

char *motion_to_string(motion_t motion) {
  switch (motion) {
  case STOP:
    return "STOP";
  case FORWARD:
    return "FORWARD";
  case LEFT:
    return "LEFT";
  case RIGHT:
    return "RIGHT";
  default:
    return "<invalid motion>";
  }
}

typedef enum action {
  CATCHED,
  SEARCHING,
  RANDOMLY,
  RUNNING,
  CATCHING
} action_t;

char *action_to_string(action_t action) {
  switch (action) {
  case CATCHED:
    return "CATCHED";
  case SEARCHING:
    return "SEARCHING OTHERS";
  case RUNNING:
    return "RUNNING AWAY";
  case RANDOMLY:
    return "MOVING RANDOMLY";
  case CATCHING:
    return "CATCHING";
  default:
    return "<invalid action>";
  }
}

typedef enum {
  CONNECT,
  CONNECTED,
  CHOOSE_WITCH,
  CHOOSE_TARGET=240,
  BEFORE_GAME,
  PLAY,
  END_GAME,
  BROADCAST_MSGS
} phases_t;

char *phase_to_string(phases_t phase) {
  switch (phase) {
  case CONNECT:
    return "SEARCHING AND CONNECTING";
  case CONNECTED:
    return "CONNECTED TO OTHERS";
  case CHOOSE_WITCH:
    return "CHOOSE WITCH";
  case CHOOSE_TARGET:
    return "CHOOSE TARGET";
  case BEFORE_GAME:
    return "WAITING TO START";
  case PLAY:
    return "PLAYING";
  case END_GAME:
    return "END GAME";
  case BROADCAST_MSGS:
    return "BROADCASTING MESSAGES";
  default:
    return "<invalid phase>";
  }
}

// declare variables

typedef struct {
  // the color of this bot
  uint8_t my_color;
  // current distance from another bot
  uint8_t cur_distance;
  // previous distance from another bot
  uint8_t cur_position;
  // distance from the target bot
  uint8_t distance_to_target;
  // distance from following bot to target
  uint8_t following_distance_to_target;
  // value of the clock when last message was received
  uint32_t last_reception_time;
  // whether there is a new received message or not
  bool is_new_message;
  // the direction to which the bot is currently aiming
  motion_t curr_direction;
  // whether a bot should send messages or not
  bool stop_message;
  // whether a bot has catched its target
  bool target_catched;
  // for movement
  int t;
  // for spiral movement
  int i;
  // the color of the target bot
  uint8_t target_color;
  // the color of the bot that this bot is currently following (maybe is the
  // target, maybe not)
  uint8_t following_color;
  // the last distance measurment received
  distance_measurement_t dist;
  // the message to be transmitted
  message_t transmit_msg;
  // last received message
  message_t received_msg;
  // for debugging
  action_t currently_doing;
  phases_t phase;
  uint16_t connections;
  bool i_am_the_witch;
  bool moving;
} USERDATA;
