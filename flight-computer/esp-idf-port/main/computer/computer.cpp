#include "computer.h"

#include <algorithm>
#include <cstdio>

namespace seds {

Expected<std::monostate> FlightComputer::init() {
    // do we need to format?
    char data[] = "accel x, accel y, accel z, degrees x, degrees y, degrees z\n";
    return this->sd.create_file(FlightComputer::filename, (uint8_t *)data, sizeof(data)-1); // subtract one
}

void FlightComputer::process() {
    while (true) {
        // just collect imu for now
        auto imu_data_try = this->imu.read_imu();
        if (!imu_data_try.has_value()) {
            continue;
        }
        IMUData imu_data = unwrap(std::move(imu_data_try));
        // buffer is 6 floats, each max 20 char (overestimate with %s), plus 5 commas, newline, and zero terminator
        char buffer[6 * 20 + 5 + 1 + 1];
        std::fill(std::begin(buffer), std::begin(buffer) + sizeof(buffer), 'X');
        size_t idx = 0;
        idx += snprintf(buffer, 20, "%g", imu_data.ax);
        buffer[idx] = ','; // write over previous null terminator
        idx += snprintf(&buffer[idx+1], 20, "%g", imu_data.ay); 
        buffer[idx] = ','; 
        idx += snprintf(&buffer[idx+1], 20, "%g", imu_data.az);
        buffer[idx] = ','; 
        idx += snprintf(&buffer[idx+1], 20, "%g", imu_data.gx);
        buffer[idx] = ','; 
        idx += snprintf(&buffer[idx+1], 20, "%g", imu_data.gy);
        buffer[idx] = ','; 
        idx += snprintf(&buffer[idx+1], 20, "%g", imu_data.gz);
        buffer[idx] = ','; 
        buffer[idx + 1] = '\n';
        buffer[idx + 2] = '\0';

        // if this takes too long we should rework the api to hand out file descriptors
        // no need to flush file since this function calls fclose
        // if we switch to file descriptors we need to explicitly flush
        sd.append_file(FlightComputer::filename, (uint8_t *)buffer, idx + 1); // add one to include newline but not null terminator
        // TODO ADD TIMESTAMP AND OTHER DATA
    }
}

}