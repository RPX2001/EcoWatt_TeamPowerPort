/**
 * @file Uploader.cpp
 * @brief Implementation of the Uploader class for uploading inverter samples to a cloud service.
 *
 * @details
 * Implements the Uploader class methods, providing a wrapper around a CloudStub (or real cloud interface)
 * to manage the process of sending batches of inverter measurements. Designed for testability and
 * dependency injection in embedded or simulated environments.
 *
 * @author Yasith
 * @author Prabath
 * @version 1.0
 * @date 2025-08-18
 *
 * @par Revision history
 * - 1.0 (Yasith, 2025-08-18) Moved implementations to cpp file and split to layers.
 */


#include "Uploader.h"

/**
 * @fn Uploader::Uploader
 * @briefConstructor that binds the uploader to a given cloud interface.
 *
 * @param cloud [in] Reference to a CloudStub instance that will perform the uploads.
 *
 * @note The referenced CloudStub instance must outlive the Uploader instance.
 */
Uploader::Uploader(CloudStub& cloud) : cloud_(cloud) {}

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
