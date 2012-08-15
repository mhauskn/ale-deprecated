/* *****************************************************************************
 * A.L.E (Arcade Learning Environment)
 * Copyright (c) 2009-2012 by Yavar Naddaf, Joel Veness, Marc G. Bellemare and 
 *   the Reinforcement Learning and Artificial Intelligence Laboratory
 * Released under the GNU General Public License; see License.txt for details. 
 *
 * Based on: Stella  --  "An Atari 2600 VCS Emulator"
 * Copyright (c) 1995-2007 by Bradford W. Mott and the Stella team
 *
 * *****************************************************************************
 *
 * RomSettings.cpp
 *
 * The interface to describe games as RL environments. It provides terminal and
 *  reward information.
 * *****************************************************************************
 */
#include "RomSettings.hpp"

ActionVect RomSettings::actions;
ActionVect RomSettings::all_actions;

ActionVect& RomSettings::getAvailableActions() {
  // Default action set
  if (actions.empty()) {
    for (int a = 0; a < PLAYER_B_NOOP; a++)
      actions.push_back((Action)a);
  }

  return actions;
}

ActionVect& RomSettings::getAllActions() {
  // Generate the set of all actions
  if (all_actions.empty()) {
    for (int a = 0; a < PLAYER_B_NOOP; a++)
      all_actions.push_back((Action)a);
  }

  return all_actions;
}
