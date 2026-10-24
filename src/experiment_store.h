#pragma once

#include <string>
#include <vector>

#include "experiment_metrics.h"

class ExperimentStore {
public:
    explicit ExperimentStore(std::string path);

    bool load(std::vector<ExperimentRecord>& records, std::string& error) const;
    bool append(const ExperimentRecord& record, std::string& error) const;
    const std::string& path() const;

private:
    std::string path_;
};
