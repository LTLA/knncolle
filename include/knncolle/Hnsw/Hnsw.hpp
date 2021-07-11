#ifndef KNNCOLLE_HNSW_HPP
#define KNNCOLLE_HNSW_HPP

#include "../utils/knn_base.hpp"
#include "../utils/NeighborQueue.hpp"

#include "hnswlib/hnswalg.h"
#include <cmath>

/**
 * @file Hnsw.hpp
 *
 * Implements an approximate nearest neighbor search with HNSW.
 */

namespace knncolle {

/**
 * @brief Perform an approximate nearest neighbor search with HNSW.
 *
 * @tparam DISTANCE An **hnswlib**-derived class to compute the distance between vectors.
 * Note that this is not the same as the classes in `distances.hpp`.
 * @tparam INDEX_t Integer type for the indices.
 * @tparam DISTANCE_t Floating point type for the distances.
 */
template<class SPACE, typename INDEX_t = int, typename DISTANCE_t = double, typename QUERY_t = double>
class HnswSearch : public knn_base<INDEX_t, DISTANCE_t, QUERY_t> {
    typedef float INTERNAL_DATA_t; // floats are effectively hard-coded into hnswlib, given that L2Space only uses floats.

public:
    INDEX_t nobs() const {
        return num_obs;
    }
    
    INDEX_t ndim() const {
        return num_dim;
    }

public:
    /**
     * @param ndim Number of dimensions.
     * @param nobs Number of observations.
     * @param vals Pointer to an array of length `ndim * nobs`, corresponding to a dimension-by-observation matrix in column-major format, 
     * i.e., contiguous elements belong to the same observation.
     *
     * @tparam INPUT Floating-point type of the input data.
     */
    template<typename INPUT>
    HnswSearch(INDEX_t ndim, INDEX_t nobs, const INPUT* vals, int nlinks = 16, int ef_construction= 200, int ef_search = 10) : 
        space(ndim), hnsw_index(&space, nobs, nlinks, ef_construction), num_dim(ndim), num_obs(nobs)
    {
        if constexpr(std::is_same<INPUT, INTERNAL_DATA_t>::value) {
            for (INDEX_t i=0; i < nobs; ++i, vals += ndim) {
                hnsw_index.addPoint(vals, i);
            }
        } else {
            std::vector<INTERNAL_DATA_t> copy(ndim);
            for (INDEX_t i=0; i < nobs; ++i, vals += ndim) {
                std::copy(vals, vals + ndim, copy.begin());
                hnsw_index.addPoint(copy.data(), i);
            }
        }
        hnsw_index.setEf(ef_search);
        return;
    }

    std::vector<std::pair<INDEX_t, DISTANCE_t> > find_nearest_neighbors(INDEX_t index, int k) const {
        auto V = hnsw_index.getDataByLabel<INTERNAL_DATA_t>(index);
        auto Q = hnsw_index.searchKnn(V.data(), k+1);
        auto output = harvest_queue<INDEX_t, DISTANCE_t>(Q, true, index);
        normalize(output);
        return output;
    }
        
    std::vector<std::pair<INDEX_t, DISTANCE_t> > find_nearest_neighbors(const QUERY_t* query, int k) const {
        if constexpr(std::is_same<QUERY_t, INTERNAL_DATA_t>::value) {
            auto Q = hnsw_index.searchKnn(query, k);
            auto output = harvest_queue<INDEX_t, DISTANCE_t>(Q);
            normalize(output);
            return output;
        } else {
            std::vector<INTERNAL_DATA_t> copy(query, query + num_dim);
            auto Q = hnsw_index.searchKnn(copy.data(), k);
            auto output = harvest_queue<INDEX_t, DISTANCE_t>(Q);
            normalize(output);
            return output;
        }
    }

private:
    SPACE space;
    hnswlib::HierarchicalNSW<INTERNAL_DATA_t> hnsw_index;
    INDEX_t num_dim, num_obs;

    static void normalize(std::vector<std::pair<INDEX_t, DISTANCE_t> >& results) {
        for (auto& d : results) {
            d.second = SPACE::normalize(d.second);
        }
        return;
    }
};

namespace hnsw_distances {

class Manhattan : public hnswlib::SpaceInterface<float> {
    size_t data_size_;
    size_t dim_;
public:
    Manhattan(size_t ndim) : data_size_(ndim * sizeof(float)), dim_(ndim) {}

    ~Manhattan() {}

    size_t get_data_size() {
        return data_size_;
    }

    hnswlib::DISTFUNC<float> get_dist_func() {
        return L1;
    }

    void * get_dist_func_param() {
        return &dim_;
    }

    static float L1(const void *pVect1v, const void *pVect2v, const void *qty_ptr) {
        //return *((float*)pVect2);
        const float* pVect1=static_cast<const float*>(pVect1v);
        const float* pVect2=static_cast<const float*>(pVect2v);
        size_t qty = *((size_t *) qty_ptr);
        float res = 0;
        for (; qty > 0; --qty, ++pVect1, ++pVect2) {
            res += std::fabs(*pVect1 - *pVect2);
        }
        return res;
    }
    
    static float normalize(float raw) {
        return raw;
    }
};

class Euclidean : public hnswlib::L2Space {
public:
    Euclidean(size_t ndim) : hnswlib::L2Space(ndim) {}

    static float normalize(float raw) {
        return std::sqrt(raw);
    }
};

}

/**
 * Perform an Hnsw search with Euclidean distances.
 */
template<typename INDEX_t = int, typename DISTANCE_t = double, typename QUERY_t = double>
using HnswEuclidean = HnswSearch<hnsw_distances::Euclidean, INDEX_t, DISTANCE_t, QUERY_t>;

/**
 * Perform an Hnsw search with Manhattan distances.
 */
template<typename INDEX_t = int, typename DISTANCE_t = double, typename QUERY_t = double>
using HnswManhattan = HnswSearch<hnsw_distances::Manhattan, INDEX_t, DISTANCE_t, QUERY_t>;

}

#endif
