
#pragma once
#include <list>
#include <memory>
#include <mutex>  // NOLINT
#include <optional>
#include <unordered_map>
#include<unordered_set>

#include "common/config.h"
#include "common/macros.h"

namespace bustub {

enum class AccessType { Unknown = 0, Lookup, Scan, Index };

enum class ArcStatus { MRU, MFU, MRU_GHOST, MFU_GHOST };

struct FrameStatus { // i can get it using frame_id
  
  page_id_t page_id_;

  bool evictable_;

  ArcStatus arc_status_;

  std::list<frame_id_t>::iterator alive_iter_;

  std::list<page_id_t>::iterator ghost_iter_;
  
  FrameStatus(page_id_t pid, bool ev, ArcStatus st)
      : page_id_(pid), evictable_(ev), arc_status_(st) {}
};
class ArcReplacer {
 public:
  explicit ArcReplacer(size_t num_frames);

  DISALLOW_COPY_AND_MOVE(ArcReplacer);

  
  ~ArcReplacer() = default;

  auto Evict() -> std::optional<frame_id_t>;
  void RecordAccess(frame_id_t frame_id, page_id_t page_id, AccessType access_type = AccessType::Unknown);
  void SetEvictable(frame_id_t frame_id, bool set_evictable);
  void Remove(frame_id_t frame_id);
  auto Size() -> size_t;

 private:
  std::list<frame_id_t> mru_;
  std::list<frame_id_t> mfu_;
  std::list<page_id_t> mru_ghost_;
  std::list<page_id_t> mfu_ghost_;

  
  // frame_id -> frame_status
  std::unordered_map<frame_id_t, std::shared_ptr<FrameStatus>> alive_map_;
  
  //page_id -> frame_status
  std::unordered_map<page_id_t, std::shared_ptr<FrameStatus>> ghost_map_;


   size_t evictable_frames{0};

   size_t mru_target_size_{0};

   size_t capicity;

  std::mutex latch_;

};

}   
