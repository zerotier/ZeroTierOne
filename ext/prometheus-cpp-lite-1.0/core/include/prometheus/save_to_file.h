#pragma once

#include <thread>
#include <chrono>
#include <string>
#include <fstream>
#include <memory>

#include "registry.h"
#include "text_serializer.h"

namespace prometheus {
  class SaveToFile {
    std::chrono::seconds      period        { 1 };
    std::string               filename;
    std::thread               worker_thread { &SaveToFile::worker_function, this };
    std::shared_ptr<Registry> registry_ptr  { nullptr };
    bool                      must_die      { false };

    void save_data() {
      if (registry_ptr) {
        std::fstream out_file_stream;
        out_file_stream.open(filename, std::fstream::out | std::fstream::binary);
        if (out_file_stream.is_open()) {
          TextSerializer::Serialize(out_file_stream, registry_ptr->Collect());
          out_file_stream.close();
        }
      }
    }

    void worker_function() {
      // it need for fast shutdown this thread when SaveToFile destructor is called
      const uint64_t divider = 100;
      uint64_t fraction = divider;
      for (;;) {
        std::chrono::milliseconds period_ms
          = std::chrono::duration_cast<std::chrono::milliseconds>(period);
        std::this_thread::sleep_for( period_ms / divider );
        if (must_die) {
          save_data();
          return;
        }
        if (--fraction == 0) {
          fraction = divider;
          save_data();
        }
      }
    }
    
  public:
    SaveToFile() = default;

    ~SaveToFile() {
      must_die = true;
      worker_thread.join();
    }

    SaveToFile(std::shared_ptr<Registry>& registry_, const std::chrono::seconds& period_, const std::string& filename_) {
      set_registry(registry_);
      set_delay(period_);
      set_out_file(filename_);
    }

    void set_delay (const std::chrono::seconds& new_period) {
      period = new_period;
    }


    bool set_out_file (const std::string& filename_) {
      filename = filename_;
      std::fstream out_file_stream;
      out_file_stream.open(filename, std::fstream::out | std::fstream::binary);
      bool open_success = out_file_stream.is_open();
      out_file_stream.close();
      return open_success;
    }

    void set_registry (std::shared_ptr<Registry>& new_registry_ptr) {
      registry_ptr = new_registry_ptr;
    }

  };
}
