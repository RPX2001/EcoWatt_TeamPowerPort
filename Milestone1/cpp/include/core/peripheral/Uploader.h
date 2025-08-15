#pragma once

#include "sim/CloudStub.h"
#include "sim/InverterSim.h"

#include <vector>

/**
 * @class Uploader
 * @brief Handles uploading of inverter samples to a cloud service.
 *
 * @details
 * This class acts as a thin wrapper around a CloudStub (or real cloud interface)
 * to manage the process of sending batches of inverter measurements.
 * It is designed for testability and dependency injection.
 */
class Uploader
{
    public:
        /**
         * @fn Uploader::Uploader
         * @briefConstructor that binds the uploader to a given cloud interface.
         *
         * @param cloud [in] Reference to a CloudStub instance that will perform the uploads.
         *
         * @note The referenced CloudStub instance must outlive the Uploader instance.
         */
        explicit Uploader(CloudStub& cloud);

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
        bool upload_once(const std::vector<Sample>& batch);

    private:
        CloudStub& cloud_;
};
