#include "PlugInDevice.hpp"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_endpoint.h>

#include <cstdint>

PlugInDevice::PlugInDevice(const char *device_name, PluginAccessoryInterface *plugInAccessory,
                           esp_matter::endpoint_t *aggregator)
    : BaseDevice() {
  // Create the PlugInAccessory instance
  accessory = plugInAccessory;

  // Set up the callback for reporting attributes
  accessory->setReportAppCallback([](void *self) { static_cast<PlugInDevice *>(self)->reportEndpoint(); },
                                  this);

  // Check if an aggregator is provided
  if (aggregator != nullptr) {
    esp_matter::endpoint::bridged_node::config_t bridged_node_config;
    uint8_t flags = esp_matter::endpoint_flags::ENDPOINT_FLAG_BRIDGE |
                    esp_matter::endpoint_flags::ENDPOINT_FLAG_DESTROYABLE;
    endpoint = esp_matter::endpoint::bridged_node::create(esp_matter::node::get(), &bridged_node_config,
                                                          flags, this);
    if (device_name != nullptr && strlen(device_name) > 0 &&
        strlen(device_name) < 64)  // TODO: change to a define
    {
      strncpy(this->name, device_name, sizeof(this->name));
      ESP_LOGI(__FILENAME__, "Creating Bridged Node PlugInDevice with name: %s", name);
      esp_matter::cluster_t *bridge_device_basic_information_cluster =
          esp_matter::cluster::get(endpoint, chip::app::Clusters::BridgedDeviceBasicInformation::Id);
      esp_matter::cluster::bridged_device_basic_information::attribute::create_node_label(
          bridge_device_basic_information_cluster, name, strlen(name));
    } else {
      ESP_LOGW(__FILENAME__, "device_name is not set");
      ESP_LOGI(__FILENAME__, "Creating Bridged Node PlugInDevice with default name");
    }
    esp_matter::endpoint::set_parent_endpoint(endpoint, aggregator);
  } else {
    ESP_LOGI(__FILENAME__, "Creating PlugInDevice standalone endpoint");
    uint8_t flags = esp_matter::endpoint_flags::ENDPOINT_FLAG_NONE;
    endpoint = esp_matter::endpoint::create(esp_matter::node::get(), flags, this);
  }

  esp_matter::endpoint::on_off_plugin_unit::config_t on_off_plugin_unit_config;
  esp_matter::endpoint::on_off_plugin_unit::add(endpoint, &on_off_plugin_unit_config);

  setAccessoryPowerState(getEndpointPowerState());
}

esp_err_t PlugInDevice::updateAccessory() {
  bool powerState = getEndpointPowerState();

  ESP_LOGI(__FILENAME__, "Updating PlugInDevice accessory state to %s", powerState ? "on" : "off");

  setAccessoryPowerState(powerState);
  return ESP_OK;
}

esp_err_t PlugInDevice::reportEndpoint() {
  bool powerState = getAccessoryPowerState();

  ESP_LOGI(__FILENAME__, "Reporting PlugInDevice endpoint state to %s", powerState ? "on" : "off");

  setEndpointPowerState(powerState);
  return ESP_OK;
}

esp_err_t PlugInDevice::identify() {
  ESP_LOGI(__FILENAME__, "Identifying PlugInDevice");

  accessory->identifyYourSelf();
  return ESP_OK;
}

bool PlugInDevice::getAccessoryPowerState() { return accessory->getPower(); }

void PlugInDevice::setAccessoryPowerState(bool powerState) { accessory->setPower(powerState); }

bool PlugInDevice::getEndpointPowerState() {
  esp_matter::cluster_t *on_off_cluster = esp_matter::cluster::get(endpoint, chip::app::Clusters::OnOff::Id);
  esp_matter::attribute_t *on_off_attribute =
      esp_matter::attribute::get(on_off_cluster, chip::app::Clusters::OnOff::Attributes::OnOff::Id);
  esp_matter_attr_val_t attr_val;
  esp_matter::attribute::get_val(on_off_attribute, &attr_val);
  return attr_val.val.b;
}

void PlugInDevice::setEndpointPowerState(bool powerState) {
  esp_matter_attr_val_t attr_val = esp_matter_bool(powerState);
  esp_matter::attribute::report(esp_matter::endpoint::get_id(endpoint), chip::app::Clusters::OnOff::Id,
                                chip::app::Clusters::OnOff::Attributes::OnOff::Id, &attr_val);
}
