#ifndef _WIN32

#pragma once

#include "aspherix_cosim_field.h"
#include "aspherix_cosim_interface.h"

#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <numeric>
#include <sstream>
#include <type_traits>
#include <vector>

namespace CoSimSocket {

// Forward declaration of the implementation class.
class CoSimSettingsImpl;

class CoSimSettings : public CoSimInterface {

public:
  CoSimSettings();
  CoSimSettings(std::shared_ptr<AspherixCoSimSocket> socket);
  ~CoSimSettings() override;
  CoSimSettings(const CoSimSettings &) = delete;            // Copy constructor
  CoSimSettings(CoSimSettings &&) noexcept;                 // Move constructor
  CoSimSettings &operator=(const CoSimSettings &) = delete; // Copy assignment
  CoSimSettings &operator=(CoSimSettings &&) noexcept;      // Move assignment

  bool operator==(const CoSimSettings &other) const;

  void clearSettings();

  template <typename T>
  void addSetting(const std::string &name, T value, SyncDirection direction = SyncDirection::kUndefined);

  template <typename T>
  void setSetting(const std::string &name, T new_value);
  
  template <typename T>
  auto getSetting(const std::string &name) const -> T;

  std::size_t length(const SyncDirection &direction = SyncDirection::kUndefined) const override;
  void fromByteVector(const std::vector<char> &byte_array, std::size_t offset = 0,
                      const SyncDirection &direction = SyncDirection::kUndefined) override;
  std::vector<char> toByteVector(const SyncDirection &direction = SyncDirection::kUndefined) const override;

  void printInfo() const;

private:
  std::unique_ptr<CoSimSettingsImpl> pimpl_;
};

} // namespace CoSimSocket

#endif
