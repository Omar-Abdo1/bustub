//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// disk_scheduler.cpp
//
// Identification: src/storage/disk/disk_scheduler.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/disk/disk_scheduler.h"
#include <vector>
#include "common/macros.h"
#include "storage/disk/disk_manager.h"

namespace bustub {

DiskScheduler::DiskScheduler(DiskManager *disk_manager) : disk_manager_(disk_manager) {
  // Spawn the background thread
  background_thread_.emplace([&] { StartWorkerThread(); });
}

DiskScheduler::~DiskScheduler() {
  // Put a `std::nullopt` in the queue to signal to exit the loop
  request_queue_.Put(std::nullopt);
  if (background_thread_.has_value()) {
    background_thread_->join();
  }
}


void DiskScheduler::Schedule(std::vector<DiskRequest> &requests) {
    
  std::sort(requests.begin(),requests.end(),[&](const auto & a,const auto &b){
       return a.page_id_<b.page_id_;
  });
  
  for(auto &req : requests){
      request_queue_.Put(std::make_optional<DiskRequest>(std::move(req)));
    }
}


void DiskScheduler::StartWorkerThread() {
     
    while(true){
       
      auto req_opt = request_queue_.Get();

      if(!req_opt.has_value())break;

      DiskRequest &req = req_opt.value();

      if(req.is_write_){
        disk_manager_->WritePage(req.page_id_,req.data_);
      }
      else{
          disk_manager_->ReadPage(req.page_id_,req.data_); // read the content of the page_id into the req_data 
      }

      req.callback_.set_value(true);
    }
}

}  // namespace bustub
