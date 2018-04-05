#ifndef VG_FEATURES_HPP_INCLUDED
#define VG_FEATURES_HPP_INCLUDED

/// \file
/// features.hpp: utilities for working with Feature and FeatureType from the VG Protobuf

#include <vg.pb.h>

#include <pb2json.hpp>

#include <vector>
#include <string>



namespace vg {

using namespace std;

// We template over Alignment and MultipathAlignment because they have the same
// features methods but no base class.

// We use a Protobuf-style pointer-for-mutable interface. All the mutator
// methods take pointers, while the read methods take const references.

////////////////////////////////////////////////////////////////////////
// API
////////////////////////////////////////////////////////////////////////

/// Determine if the given alignment has ther given tag feature, or any
/// instances of the given numerical or list feature.
template<typename Item>
bool has_feature(const Item& item, const FeatureType& feature);

template<typename Item>
bool has_feature(const Item* item, const FeatureType& feature);

/// Get the numerical value of the given single-value feature on the given
/// item. Throws an error if the feature isn't present. Should not be called on
/// multi-valued features.
template<typename Item>
double get_feature(const Item& item, const FeatureType& feature);

template<typename Item>
double get_feature(const Item* item, const FeatureType& feature);

/// Get the numerical values of the given multi-valued feature, or an empty
/// vector if the feature isn't present.
template<typename Item>
vector<double> get_features(const Item& item, const FeatureType& feature);

template<typename Item>
vector<double> get_features(const Item* item, const FeatureType& feature);

/// Add the given tag feature to the given item, if not present already.
template<typename Item>
void add_feature(Item* item, const FeatureType& feature);

/// Append the given value to the given multi-valued feature.
template<typename Item>
void add_feature(Item* item, const FeatureType& feature, const double& value);

/// Set the given single-valued feature to the given value, adding it if it doesn't exist yet.
template<typename Item>
void set_feature(Item* item, const FeatureType& feature, const double& value);

/// Remove the given tag frature, or all instances of the given single- or
/// multi-valued feature, form the given item, if any are present.
template<typename Item>
void remove_feature(Item* item, const FeatureType& feature);

////////////////////////////////////////////////////////////////////////
// Implementation
////////////////////////////////////////////////////////////////////////

template<typename Item>
bool has_feature(const Item& item, const FeatureType& feature) {
    for (auto& record : item.feature()) {
        // Do a linear scan
        if (record.type() == feature) {
            // And if we find it, return it
            return true;
        }
    }
    // Otherwise we didn't find it
    return false;
}

template<typename Item>
bool has_feature(const Item* item, const FeatureType& feature) {
    return has_feature(*item, feature);
}

template<typename Item>
double get_feature(const Item& item, const FeatureType& feature) {
    for (auto& record : item.feature()) {
        // Do a linear scan
        if (record.type() == feature) {
            // And if we find it, return it
            return record.value();
        }
    }
    // Otherwise we didn't find it
    throw runtime_error("Feature " + to_string(feature) + " not found in " + pb2json(item));
}

template<typename Item>
double get_feature(const Item* item, const FeatureType& feature) {
    return get_feature(*item, feature);
}


template<typename Item>
vector<double> get_features(const Item& item, const FeatureType& feature) {
    vector<double> to_return;
    for (auto& record : item.feature()) {
        // Do a linear scan
        if (record.type() == feature) {
            // And if we find it, gather it up
            to_return.push_back(record.value());
        }
    }
    return to_return;
}

template<typename Item>
vector<double> get_features(const Item* item, const FeatureType& feature) {
    return get_features(*item, feature);
}

template<typename Item>
void add_feature(Item* item, const FeatureType& feature) {
    if (!has_feature(item, feature)) {
        // If it isn't in the list already, add it.
        Feature* added = item->add_feature();
        added->set_type(feature);
    }
}

template<typename Item>
void add_feature(Item* item, const FeatureType& feature, const double& value) {
    // Always add it
    Feature* added = item->add_feature();
    added->set_type(feature);
    // And set the value
    added->set_value(value);
}

template<typename Item>
void set_feature(Item* item, const FeatureType& feature, const double& value) {
    remove_feature(item, feature);
    add_feature(item, feature, value);
}

template<typename Item>
void remove_feature(Item* item, const FeatureType& feature) {
    for (size_t i = 0; i < item->feature_size();) {
        if (item->feature(i)->type() == feature) {
            // We need to remove it
            // So swap it last
            item->mutable_feature()->SwapElements(i, item->feature_size());
            // And remove the last element
            item->mutable_feature()->RemoveLast();
            
            // Stay here so we can look at what we swapped into this position
        } else {
            // Don't need to remove anything, so look at the next item
            i++;
        }
    }
}



}



#endif
