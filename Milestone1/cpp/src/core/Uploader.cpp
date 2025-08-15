#pragma once

#include "Uploader.h"

#include <vector>
#include "sim/CloudStub.h"
#include "sim/InverterSim.h"

/**
 * @fn Uploader::Uploader
 * @briefConstructor that binds the uploader to a given cloud interface.
 *
 * @param cloud [in] Reference to a CloudStub instance that will perform the uploads.
 *
 * @note The referenced CloudStub instance must outlive the Uploader instance.
 */
explicit Uploader::Uploader(CloudStub& cloud) : cloud_(cloud) {}

/**
 * @fn Uploader::upload_once
 * @brief Upload a single batch of inverter samples.
 *
 * @param batch [in] Vector of samples to upload.
 * @return true  If the batch was successfully uploaded.
 * @return false If the upload failed (caller should retry).
 *
 * @details
 * This method delegates the actual upload to the underlying CloudStub.
 * The upload process may block and may fail based on CloudStub’s behavior.
 */
bool Uploader::upload_once(const std::vector<Sample>& batch) 
{ 
    return cloud_.upload(batch); 
}
