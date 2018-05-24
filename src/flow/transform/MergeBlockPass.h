// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

namespace flow {
  class IRHandler;
}

namespace flow::transform {

/**
 * Merges equal blocks into one, eliminating duplicated blocks.
 *
 * A block is equal if their instructions and their successors are equal.
 */
bool mergeSameBlocks(IRHandler* handler);

} // namespace flow::transform

