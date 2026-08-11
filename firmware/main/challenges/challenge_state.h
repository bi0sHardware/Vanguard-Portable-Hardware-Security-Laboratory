#pragma once

// Runtime/NVS-persisted completion tracking — separate from challenge_data.h
// since state is mutable/per-badge while that file is const shared content.
namespace challenges {

bool isCompleted(const char* id);
void setCompleted(const char* id, bool done);

} // namespace challenges
