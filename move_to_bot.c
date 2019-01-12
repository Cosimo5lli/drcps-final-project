#include <math.h>
#include <stdbool.h>

#include <kilombo.h>

#include "move_to_bot.h"

#ifdef SIMULATOR
#include <stdio.h> // for printf
#else
#include <avr/io.h> // for microcontroller register defs
//  #define DEBUG          // for printf to serial port
//  #include "debug.h"
#endif

REGISTER_USERDATA(USERDATA)

/* Helper function for setting motor speed smoothly
 */
void smooth_set_motors(uint8_t ccw, uint8_t cw) {
  // OCR2A = ccw;  OCR2B = cw;
#ifdef KILOBOT
  uint8_t l = 0, r = 0;
  if (ccw && !OCR2A) // we want left motor on, and it's off
    l = 0xff;
  if (cw && !OCR2B) // we want right motor on, and it's off
    r = 0xff;
  if (l || r) // at least one motor needs spin-up
  {
    set_motors(l, r);
    delay(15);
  }
#endif
  // spin-up is done, now we set the real value
  set_motors(ccw, cw);
}

void set_motion(motion_t new_motion) {
  switch (new_motion) {
  case STOP:
    smooth_set_motors(0, 0);
    break;
  case FORWARD:
    smooth_set_motors(kilo_turn_left, kilo_turn_right);
    break;
  case LEFT:
    smooth_set_motors(kilo_turn_left, 0);
    break;
  case RIGHT:
    smooth_set_motors(0, kilo_turn_right);
    break;
  }
  mydata->curr_direction = new_motion;
}

// clean the array of transmit message and the array of
// received message.
void clean_array_SR() {
  for (int i = 0; i < 4; i++) {
    mydata->transmit_msg.data[i] = 0;
    mydata->received_msg.data[i] = 0;
  }
}

uint8_t get_color_for_id(uint8_t id) {
  return RGB(kilo_uid, (3 - kilo_uid), ((6 - kilo_uid) / 2));
  // return id;
}

void assign_color() {
  mydata->my_color = get_color_for_id(kilo_uid);
  set_color(mydata->my_color);
}

void move_direction_for_seconds0(motion_t direction, uint8_t seconds) {
  if (mydata->t * 32 < kilo_ticks &&
      kilo_ticks < (mydata->t + seconds / 2) * 32) {
    set_motion(direction);
    // skip seconds
    mydata->t += seconds;
  } else if (kilo_ticks > (mydata->t + seconds) * 32) {
    mydata->t = kilo_ticks / 32;
  }
}

void move_direction_for_seconds(motion_t direction, uint8_t seconds) {
  // mydata->t = (kilo_ticks / 32);
  // printf("t, kiloticks: %i, %i\n", mydata->t*32, kilo_ticks);
  if (!mydata->moving && mydata->t * 32 < kilo_ticks &&
      kilo_ticks < (mydata->t + seconds) * 32) {
    set_motion(direction);
    mydata->moving = true;
    // skip seconds
    // mydata->t += seconds;
  } else if (mydata->moving && (mydata->t * 32 > kilo_ticks ||
                                kilo_ticks > (mydata->t + seconds) * 32)) {
    mydata->t = kilo_ticks / 32;
    mydata->moving = false;
  }
  //  else if (kilo_ticks > (mydata->t + seconds) * 32) {
  //   mydata->t = kilo_ticks / 32;
  // }
}

void move_random_direction() {
  // actually, random int from 1 to 3, 1 = FORWARD, 2 = LEFT and 3 = RIGHT
  move_direction_for_seconds((rand_soft() % 3) + 1, 3);
  mydata->currently_doing = RANDOMLY;
}

void change_direction_based_on_distance(bool run_away) {
  if (run_away ? mydata->cur_distance < mydata->cur_position
               : mydata->cur_distance > mydata->cur_position) {
    if (mydata->curr_direction == LEFT) {
      move_direction_for_seconds(RIGHT, 2);
    } else if (mydata->curr_direction == RIGHT) {
      move_direction_for_seconds(LEFT, 2);
    } else {
      move_direction_for_seconds((rand_soft() % 2 + 2), 2);
    }
  } else if (mydata->cur_distance == mydata->cur_position) {
    move_direction_for_seconds(FORWARD, 2);
  }
}

void catch_other_bot() {
  mydata->currently_doing = CATCHING;
  change_direction_based_on_distance(false);
}

void run_away_from_others() {
  mydata->currently_doing = RUNNING;
  change_direction_based_on_distance(true);
}

void move_to_find_other_bots() {
  mydata->currently_doing = SEARCHING;
  if (kilo_ticks > 32 * (mydata->t) && kilo_ticks < 32 * (mydata->t + 3)) { // diminuire qui allarga spirale
    set_motion(RIGHT);
  } else if (kilo_ticks > 32 * (mydata->t + 3) &&
             kilo_ticks < 32 * (mydata->t + 6)) { // aumentare questo allarga spirale
    set_motion(FORWARD);
    mydata->t = mydata->t + mydata->i;
    mydata->i = mydata->i + SPIRAL_INCREMENT; //modificare questo cambia forma spirale in modo molto casuale
  } else if (kilo_ticks > 32 * (mydata->t + mydata->i + 3)) {
    // reset things if t has remained set to a previous value that is too low
    // and would cause the bot to always turn right
    mydata->t = kilo_ticks / 32;
    mydata->i = 1;
  }
  // reset the spiral if it's too much that the bot is swirling around
  if ((kilo_ticks - mydata->last_reception_time) % (SECONDS_RESET_SPIRAL * 32) <
      3) {
    mydata->i = 1;
  }
}

void set_catched() {
  mydata->target_catched = true;
  set_motion(STOP);
  set_color(RGB(0, 0, 0));
}

void update_distance_estimate() {
  // ignore garbage messages
  if (mydata->received_msg.data[0] > 63) {
    return;
  }
  uint8_t distance_estimate = estimate_distance(&mydata->dist);
  if (mydata->target_color == mydata->my_color) {
    // the target consider everyone else a target (whoever catches him, he must
    // stop)
    mydata->distance_to_target = distance_estimate < mydata->distance_to_target
                                     ? distance_estimate
                                     : mydata->distance_to_target;
    mydata->cur_position = mydata->cur_distance;
    mydata->cur_distance = distance_estimate;
    mydata->following_color = mydata->received_msg.data[0];
  } else {
    // choose whether to update who to follow (the bot who is believed to be the
    // closest to target)
    if (mydata->received_msg.data[1] < mydata->following_distance_to_target) {
      mydata->following_color = mydata->received_msg.data[0];
      mydata->following_distance_to_target = mydata->received_msg.data[1];
    }

    // then it update the distance estimate
    if (mydata->received_msg.data[0] == mydata->following_color) {
      // update distance estimate
      mydata->cur_position = mydata->cur_distance;
      mydata->cur_distance = distance_estimate;
      // update distance to target: it consider the sum of the distance from the
      // bot
      // it's currently following plus its distance from the target
      if (mydata->following_distance_to_target + distance_estimate < 255) {
        mydata->distance_to_target =
            mydata->following_distance_to_target + distance_estimate;
      } else {
        // prevent overflow (maybe also use uint16_t instead of 8?)
        mydata->distance_to_target = 255;
      }
    }
  }
  if (mydata->distance_to_target < RANGE_TO_TOUCH) {
    set_catched();
  }
}

void play_the_game() {
  if (mydata->is_new_message) {
    mydata->is_new_message = false;
    update_distance_estimate();
  }

  if (mydata->target_catched) {
    mydata->currently_doing = CATCHED;
    return;
  }
  // if no message has been received for a long time
  bool out_of_range =
      mydata->last_reception_time + TIME_TO_CONSIDER_OUT_OF_RANGE <= kilo_ticks;

  // out of range: target bot is moving randomly, catchers are trying to find
  // him
  // || mydata->distance_to_target >= 255 <-- aggiungere questo nell'if??? Può starci, boh
  if (out_of_range) {
    // reset everything that matters for catching bots
    mydata->cur_distance = UINT8_MAX;
    mydata->cur_position = UINT8_MAX;
    mydata->distance_to_target = UINT8_MAX;
    mydata->following_distance_to_target = UINT8_MAX;
    if (mydata->my_color == mydata->target_color) {
      move_random_direction();
    } else {
      mydata->stop_message = true;
      move_to_find_other_bots();
    }
  } else
  // in range: target bot is running away, catchers are trying to catch up
  // (katchup?)
  {
    if (mydata->my_color == mydata->target_color) {
      run_away_from_others();
    } else {
      catch_other_bot();
      mydata->stop_message = false;
    }
  }
}

void update_connections() {
  mydata->connections =
      mydata->connections |
      ((mydata->received_msg.data[2] << 8) | mydata->received_msg.data[1]);
}

void send_connections() {
  mydata->transmit_msg.data[1] = mydata->connections & 255;
  mydata->transmit_msg.data[2] = mydata->connections >> 8;
}

bool first_phase() {

  // if this bot can communicate with bot 1 (even through other bots), update
  // connections, stop moving and start trasmitting
  if (mydata->received_msg.data[0] == CONNECT &&
      (mydata->received_msg.data[1] >> 1) & 1) {
    update_connections();
    set_motion(STOP);
    mydata->stop_message = false;
  }

  // populate message for others
  mydata->transmit_msg.data[0] = CONNECT;
  send_connections();

  // if this bot has not stopped, it means it cannot connect with bot 1, so look
  // for it
  if (!((mydata->connections >> 1) & 1)) {
    move_to_find_other_bots();
    return false;
  }

  // return true only if everyone is connected with everyone else (indirectly)
  return mydata->connections == 1023;
}

void reset_variables() {
  mydata->cur_distance = 0;
  mydata->is_new_message = false;
  mydata->distance_to_target = UINT8_MAX;
  mydata->following_distance_to_target = UINT8_MAX;
  mydata->last_reception_time = 0;
  mydata->target_catched = false;
  mydata->t = 0;
  mydata->i = 1;
  assign_color();
  mydata->phase = CONNECT;
  mydata->stop_message = kilo_uid == 1 ? false : true;
  mydata->connections = 1 << kilo_uid;
  clean_array_SR();
}

void setup() {
  reset_variables();
  mydata->games_counter = 1;
  rand_seed(3);
}

void loop() {
  switch (mydata->phase) {
  case CONNECT:
    if (first_phase()) {
      mydata->phase = CONNECTED;
      mydata->t = kilo_ticks;
    }
    break;
  case CONNECTED:
    if (kilo_ticks > mydata->t + 5 * 32) {
      clean_array_SR();
      mydata->phase = kilo_uid == 1 ? CHOOSE_WITCH : BROADCAST_MSGS;
    }
    break;
  case CHOOSE_WITCH:
    NULL;
    int witch = (rand_soft() % 10);
    printf("Witch: %i\n", witch);
    // clean_array_SR();
    mydata->transmit_msg.data[1] = witch;
    mydata->transmit_msg.data[0] = CHOOSE_WITCH; // phase
    mydata->phase = BROADCAST_MSGS;
    break;
  case CHOOSE_TARGET:
    NULL;
    uint8_t chosen_id = rand_soft() % 10;
    mydata->target_color = get_color_for_id(chosen_id);
    printf("Bot %i, target color: %i, target id: %i\n", kilo_uid,
           mydata->target_color, chosen_id);

    // send it to the other bots
    mydata->transmit_msg.data[0] = CHOOSE_TARGET;
    mydata->transmit_msg.data[1] = mydata->target_color;

    mydata->t = kilo_ticks;
    mydata->phase = BEFORE_GAME;
    break;
  case BEFORE_GAME:
    if (kilo_ticks > mydata->t + 10 * 32) {
      mydata->stop_message =
          mydata->my_color == mydata->target_color ? false : true;
      mydata->phase = PLAY;
      mydata->t = kilo_ticks / 32;
    }
    break;
  case PLAY:
    mydata->transmit_msg.data[0] = mydata->my_color;
    mydata->transmit_msg.data[1] = mydata->my_color == mydata->target_color
                                       ? 0
                                       : mydata->distance_to_target;
    // mydata->stop_message = false;
    play_the_game();
    if (mydata->target_catched || mydata->received_msg.data[0] == END_GAME) {
      mydata->connections = 1 << kilo_uid;
      clean_array_SR();
      mydata->phase = END_GAME;
    }
    break;
  case END_GAME:
    mydata->t = 0;
    set_catched();
    mydata->transmit_msg.data[0] = END_GAME;
    if (mydata->received_msg.data[0] == END_GAME) {
      update_connections();
    }

    send_connections();
    mydata->stop_message = false;
    if (mydata->connections == 1023) {
      mydata->t = kilo_ticks;
      mydata->phase = RESET_GAME;
    }
    break;
  case BROADCAST_MSGS:
    if ((kilo_uid != 1 || mydata->received_msg.data[0] == CHOOSE_TARGET ||
         mydata->received_msg.data[0] == CHOOSE_WITCH) &&
        mydata->received_msg.data[0]) {
      mydata->transmit_msg.data[0] = mydata->received_msg.data[0];
      mydata->transmit_msg.data[1] = mydata->received_msg.data[1];
    }
    mydata->stop_message = false;
    if (mydata->received_msg.data[0] == CHOOSE_WITCH &&
        mydata->received_msg.data[1] == kilo_uid) {
      printf("Bot %i: i'm the witch\n", kilo_uid);
      mydata->phase = CHOOSE_TARGET;
    }
    if (mydata->received_msg.data[0] == CHOOSE_TARGET) {
      mydata->target_color = mydata->received_msg.data[1];
      mydata->t = kilo_ticks;
      mydata->phase = BEFORE_GAME;
    }
    break;
  case RESET_GAME:
    if (kilo_ticks > mydata->t + 5 * 32) {
      if (mydata->games_counter < NUM_OF_GAMES) {
        mydata->games_counter++;
        reset_variables();
        mydata->phase = CONNECT;
      } else {
        set_color(RGB(3, 3, 3));
        mydata->stop_message = true;
      }
    }
    break;
  }
}

void message_rx(message_t *m, distance_measurement_t *d) {
  mydata->is_new_message = true;
  // start transmitting
  // mydata->stop_message = false;
  // set the time of reception of this message
  mydata->last_reception_time = kilo_ticks;
  mydata->dist = *d;
  mydata->received_msg = *m;
}

void setup_message(void) {
  mydata->transmit_msg.type = NORMAL;
  // mydata->transmit_msg.data[0] = mydata->my_color;
  // mydata->transmit_msg.data[1] =
  // mydata->my_color == mydata->target_color ? 0 :
  // mydata->distance_to_target; finally, calculate a message check sum
  mydata->transmit_msg.crc = message_crc(&mydata->transmit_msg);
}

message_t *message_tx() {
  if (!mydata->stop_message) {
    setup_message();
    return &mydata->transmit_msg;
  } else {
    return NULL;
  }
}

#ifdef SIMULATOR
/* provide a text string for the simulator status bar about this bot */
static char botinfo_buffer[10000];
char *cb_botinfo(void) {
  char *p = botinfo_buffer;
  p += sprintf(p, "ID: %d, ", kilo_uid);
  p += sprintf(p, "Direction: %s", motion_to_string(mydata->curr_direction));
  p += sprintf(p, ", Dtt: %i, sending: [%i, %i, %i, %i]\n",
               mydata->distance_to_target, mydata->transmit_msg.data[0],
               mydata->transmit_msg.data[1], mydata->transmit_msg.data[2],
               mydata->transmit_msg.data[3]);
  p += sprintf(p, "color: %i, phase: %s, receiving: [%i, %i, %i, %i]\n",
               mydata->my_color, phase_to_string(mydata->phase),
               mydata->received_msg.data[0], mydata->received_msg.data[1],
               mydata->received_msg.data[2], mydata->received_msg.data[3]);
  p += sprintf(p, "target: %i, following: %i, t: %i, i: %i, doing %s\n",
               mydata->target_color, mydata->following_color, mydata->t,
               mydata->i, action_to_string(mydata->currently_doing));
  return botinfo_buffer;
}
#endif

int main() {
  kilo_init();
  kilo_message_rx = message_rx;
  SET_CALLBACK(botinfo, cb_botinfo);
  kilo_message_tx = message_tx;
  kilo_start(setup, loop);
  return 0;
}
