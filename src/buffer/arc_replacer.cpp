#include "buffer/arc_replacer.h"
#include <optional>
#include "common/config.h"

namespace bustub {


    /*
    If the MRU list size is smaller than the target size, 
    we try to evict from the MFU list. 
    
    If the MRU list size is greater than or equal to the target size, 
    we try to evict from the MRU list.
    
    In either case, if eviction is not possible from the intended side 
    (nothing is evictable in that list), 
    try evicting from the other list.
    */
ArcReplacer::ArcReplacer(size_t num_frames) : capicity(num_frames) {}


auto ArcReplacer::Evict() -> std::optional<frame_id_t> { 
    std::scoped_lock<std::mutex> lock(latch_);

    if(evictable_frames == 0) {
        return std::nullopt;
    }

    auto try_evict_from = [&](std::list<frame_id_t>& list, ArcStatus status) -> std::optional<int> {
        for (auto it = list.rbegin(); it != list.rend(); ++it) {
            frame_id_t frame_id = *it;
            auto frame_status = alive_map_[frame_id];
            
            if (frame_status->evictable_) {
                page_id_t page_id = frame_status->page_id_;
                
                list.erase(std::next(it).base()); // remove the reverse iterator in a normal way
                alive_map_.erase(frame_id);
                evictable_frames--;

                if (status == ArcStatus::MRU) {
                    mru_ghost_.push_front(page_id);
                    ghost_map_[page_id] = std::make_shared<FrameStatus>(page_id, false, ArcStatus::MRU_GHOST);
                    ghost_map_[page_id]->ghost_iter_ = mru_ghost_.begin();
                } else {
                    mfu_ghost_.push_front(page_id);
                    ghost_map_[page_id] = std::make_shared<FrameStatus>(page_id, false, ArcStatus::MFU_GHOST);
                    ghost_map_[page_id]->ghost_iter_ = mfu_ghost_.begin();
                }
                
                return frame_id;
            }
        }
        return std::nullopt;
    };

    std::optional<int> ans = std::nullopt;

    if (mru_.size() < mru_target_size_) {
        ans = try_evict_from(mfu_, ArcStatus::MFU);
        if (ans == std::nullopt) ans = try_evict_from(mru_, ArcStatus::MRU);
    } else {
        ans = try_evict_from(mru_, ArcStatus::MRU);
        if (ans == std::nullopt) ans = try_evict_from(mfu_, ArcStatus::MFU);
    }
        
    return ans;
}

/*

Page already exists in MRU/MFU: 
This is the case where the actual cache hits. Move the page to the front of MFU.

Page already exists in MRU ghost: 
This is the case where the actual cache misses but we hit on the ghost list.
 In this case we treat it as a pseudo-hit and adapt the target size.
  If the size of the MRU ghost list is greater than or equal to the size of the MFU ghost list,
   increase the MRU target size by one. Else increase it by MFU ghost size / MRU ghost size (rounded down).
    Do not increase the target size above replacer_size. Then move the page to the front of MFU.
     The rational of this is if the MRU list is a little larger, then the DBMS could have had a cache hit.


Page already exists in MFU ghost: 
Similar to the previous case, this is when the actual cache misses but we hit on the ghost list. 
If the size of the MFU ghost list is greater than or equal to the size of the MRU ghost list, 
decrease the MRU target size by 1. 
Else decrease the MRU target size by MRU ghost size / MFU ghost size (rounded down).
 Do not decrease the target size below 0. Then move the page to the front of MFU. 
 The rational of this is if the MFU list is a little larger, the DBMS could have had a cache hit.



Page is not in the replacer: 
This is the case where the actual cache misses and the ghost list misses. 
Then either of the following should happen.
 If MRU size + MRU ghost size = replacer size: Kill the last element in the MRU ghost list,
  then add the page to the front of MRU.

Else MRU size + MRU ghost size should be smaller than replacer size (it should never be larger if you do things correctly).
In this case
    If MRU size + MRU ghost size + MFU size + MFU ghost size = 2 * replacer size: 
    Kill the last element in the MFU ghost list, then add the page to the front of MRU.
            
Else simply add the page to the front of the MRU.
*/
void ArcReplacer::RecordAccess(frame_id_t frame_id, page_id_t page_id, [[maybe_unused]] AccessType access_type) {
  
        std::scoped_lock<std::mutex>lock(latch_);

    // Case 1: Page already exists in MRU / MFU
    
    if(alive_map_.find(frame_id)!=alive_map_.end()){
        
        auto FrameState = alive_map_[frame_id];
        if(FrameState->arc_status_==ArcStatus::MRU){
        mru_.erase(FrameState->alive_iter_);
        }
        else{
         mfu_.erase(FrameState->alive_iter_);
        }

         mfu_.push_front(frame_id);
         FrameState->arc_status_=ArcStatus::MFU;
         FrameState->alive_iter_ = mfu_.begin();
        return;
    }

    // Case 2: Page already exists in MRU ghost
    if(ghost_map_.find(page_id)!=ghost_map_.end() && ghost_map_[page_id]->arc_status_==ArcStatus::MRU_GHOST){
        int step = mru_ghost_.size()>=mfu_ghost_.size() ? 1 : mfu_ghost_.size()/mru_ghost_.size();
        mru_target_size_ += step;
        mru_target_size_ = std::min(mru_target_size_,capicity);

        mru_ghost_.erase(ghost_map_[page_id]->ghost_iter_);
        ghost_map_.erase(page_id);
       
        alive_map_[frame_id] = std::make_shared<FrameStatus>(page_id,false,ArcStatus::MFU);
        mfu_.push_front(frame_id);
        alive_map_[frame_id]->alive_iter_=mfu_.begin();

        return;
    }

    // Case 3: Page already exists in MFU ghost
    if(ghost_map_.find(page_id)!=ghost_map_.end() && ghost_map_[page_id]->arc_status_==ArcStatus::MFU_GHOST){
        size_t step = mfu_ghost_.size()>=mru_ghost_.size() ? 1 : mru_ghost_.size()/mfu_ghost_.size();
       
        if (mru_target_size_ > step) {
            mru_target_size_ -= step;
        } else {
            mru_target_size_ = 0;
        }

        mfu_ghost_.erase(ghost_map_[page_id]->ghost_iter_);
        ghost_map_.erase(page_id);
       
        alive_map_[frame_id] = std::make_shared<FrameStatus>(page_id,false,ArcStatus::MFU);
        mfu_.push_front(frame_id);
        alive_map_[frame_id]->alive_iter_=mfu_.begin();

        return;
    }

    // Case 4: Page is not in the replacer

    if(mru_.size()+mru_ghost_.size()==capicity){
     
        if (!mru_ghost_.empty()) {
      auto evict_page = mru_ghost_.back();
      mru_ghost_.pop_back();
      ghost_map_.erase(evict_page);
      }

    }
    else if (mru_.size() + mru_ghost_.size() < capicity) {
      if (mru_.size() + mru_ghost_.size() + mfu_.size() + mfu_ghost_.size() == 2 * capicity) {
      if (!mfu_ghost_.empty()) {
        auto evict_page = mfu_ghost_.back();
        mfu_ghost_.pop_back();
        ghost_map_.erase(evict_page);
      }
    }
}
    mru_.push_front(frame_id);
    alive_map_[frame_id] = std::make_shared<FrameStatus>(page_id,false, ArcStatus::MRU);
    alive_map_[frame_id]->alive_iter_=mru_.begin();
}


void ArcReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
   
        std::scoped_lock<std::mutex>lock(latch_);

         auto it = alive_map_.find(frame_id);
        if(it==alive_map_.end())
        return;

      bool current_evictable = it->second->evictable_;
      
      if(current_evictable==!set_evictable){
         if(!set_evictable){
            evictable_frames--;
         }
          else{
           evictable_frames++;
          }
      }
      it->second->evictable_= set_evictable;
}


void ArcReplacer::Remove(frame_id_t frame_id) {
    // i want to Remove an evictable frame from replacer.
    
        std::scoped_lock<std::mutex>lock(latch_);

        auto it = alive_map_.find(frame_id);
        if(it==alive_map_.end())
        return;
      
  if (!it->second->evictable_){
    throw std::runtime_error("Attempted to remove a non-evictable frame.");
  }
   
    if (it->second->arc_status_ == ArcStatus::MRU) {
    mru_.erase(alive_map_[frame_id]->alive_iter_);
  } else {
    mfu_.erase(alive_map_[frame_id]->alive_iter_);
  }

  evictable_frames--;
  alive_map_.erase(it);
}


auto ArcReplacer::Size() -> size_t { 
    
    std::scoped_lock<std::mutex>lock(latch_);

    return evictable_frames;
 
}

}  
