#include <vector>
#include <unordered_set>
#include <algorithm> // stable_sort
#include <numeric> // iota
#include <cmath>

#include "uvMath.h"
#include "uvQuery.h"


using std::abs;
using std::sqrt;
using std::min;
using std::max;



// Build a vector of indexes that sort another vector
template <typename T>
std::vector<size_t> argsort(const std::vector<T> &v) {
	// initialize original index locations
	std::vector<size_t> ret(v.size());
	iota(ret.begin(), ret.end(), 0);

	// sort indexes based on comparing values in v
	// using std::stable_sort instead of std::sort
	// to avoid unnecessary index re-orderings
	// when v contains elements of equal values 
	std::stable_sort(ret.begin(), ret.end(),
		[&v](size_t i1, size_t i2) {return v[i1] < v[i2];});

	return ret;
}

// Sweep line algorithm for finding which triangle contains a given query point
// but for a list of query points
void sweep(
		const std::vector<uv_t> &qPoints, // the UV query points
		const std::vector<uv_t> &uvs,     // the triangulated UV's
		const std::vector<size_t> &tris,  // the flattened triangle indexes

		std::vector<size_t> &out,         // the tri-index per qPoint
		std::vector<size_t> &missing,     // Any qPoints that weren't in a triangle
		std::vector<uv_t> &barys          // The barycentric coordinates
		
){
	// build a data structure indexing into tris based on the
	// min/max bounding box of each triangle
	size_t numTris = tris.size() / 3;
	std::vector<double> ymxs(numTris), ymns(numTris), xmxs(numTris), xmns(numTris);
	for (size_t i=0; i<numTris; ++i){
		size_t a = 3*i;
		xmns[i] = min(uvs[tris[a]][0], min(uvs[tris[a+1]][0], uvs[tris[a+2]][0]));
		ymns[i] = min(uvs[tris[a]][1], min(uvs[tris[a+1]][1], uvs[tris[a+2]][1]));
		xmxs[i] = max(uvs[tris[a]][0], max(uvs[tris[a+1]][0], uvs[tris[a+2]][0]));
		ymxs[i] = max(uvs[tris[a]][1], max(uvs[tris[a+1]][1], uvs[tris[a+2]][1]));
	}

	std::vector<double> qpx;
	for (auto & uv : uvs){
		qpx.push_back(uv[0]);
	}
	std::vector<size_t> qpSIdxs = argsort(qpx);
	std::vector<size_t> mxSIdxs = argsort(xmxs);
	std::vector<size_t> mnSIdxs = argsort(xmns);
    size_t qpSIdx, mxSIdx, mnSIdx;
    qpSIdx = mxSIdx = mnSIdx = 0;
	size_t qpIdx = qpSIdxs[qpSIdx];
	size_t mxIdx = mxSIdxs[mxSIdx];
	size_t mnIdx = mnSIdxs[mnSIdx];
	double qp = qpx[qpIdx];
	double mx = xmxs[mxIdx];
	double mn = xmns[mnIdx];

	out.resize(qPoints.size()); // the tri-index per qPoint
	missing.resize(qPoints.size()); // Any qPoints that weren't in a triangle

    // skip any triangles to the left of the first query point
    // The algorithm will stop once we hit the last query point

	std::vector<bool> skip(numTris);
	for (size_t i=0; i<xmxs.size(); ++i){
		skip[i] = qp > xmxs[i];
	}
	std::vector<bool> activeTris(numTris, false);
	std::unordered_set<size_t> atSet;

	while (true) {
		if (mn <= mx && mn <= qp){
			size_t aTriIdx = mnSIdxs[mnSIdx];
			if (!activeTris[aTriIdx] && !skip[aTriIdx]){
				activeTris[aTriIdx] = true;
				atSet.insert(aTriIdx);
			}
			mnSIdx += 1;
			if (mnSIdx != numTris){
				mnIdx = mnSIdxs[mnSIdx];
				mn = xmns[mnIdx];
			}
			else {
				mn = xmxs[mxSIdxs[-1]] + 1;
			}
		}
		else if (qp <= mx){
			// check query points between adding and removing
			uv_t qPoint = qPoints[qpIdx];
			double yv = qPoint[1];
			// a linear search is faster than more complex collections here
			bool found = false;
			for (auto &t : atSet){
				if (ymns[t] <= yv && yv <= ymxs[t]){
					if (pointInTri(qPoint, uvs[tris[3*t]], uvs[tris[3*t+1]], uvs[tris[3*t+2]])){
						out[qpIdx] = t;
						found = true;
						break;
					}
				}
			}
			if (!found){
				missing.push_back(qpIdx);
			}

			qpSIdx += 1;
			if (qpSIdx == qpSIdxs.size()){
				break; // we've reached the end
			}
			qpIdx = qpSIdxs[qpSIdx];
			qp = qpx[qpIdx];
		}
		else {
			// always remove triangles last
			size_t aTriIdx = mxSIdxs[mxSIdx];
			if (activeTris[aTriIdx] && !skip[aTriIdx]){
				activeTris[aTriIdx] = false;
				atSet.erase(aTriIdx);
			}

			mxSIdx += 1;
			if (mxSIdx != mxSIdxs.size()){
				mxIdx = mxSIdxs[mxSIdx];
				mx = xmxs[mxIdx];
			}
		}
	}
}


size_t closestBruteForceEdge(
	const std::vector<uv_t> &a,
	const std::vector<uv_t> &d,
	const std::vector<double> &dr2,
	const uv_t &pt
) {
	// Get the closest edge index via brute force
	// A is the collection of start points
	// D is the collection of direction vectors
	// DR2 is the colletion of squared lengths of the direction vectors
	// Pt is the single point to check
	double minIdx = 0;
	double minVal = std::numeric_limits<double>::max();

	for (size_t i = 0; i < a.size(); ++i) {
		const uv_t &aa = a[i];
		const uv_t &dd = d[i];

		// Get the lerp value of the projection of the point onto a->d
		// dot((pt - a[i]), d) / dr2[i]
		double lerp = (((pt[0] - aa[0]) * dd[0]) + ((pt[1] - aa[1]) * dd[1])) / dr2[i];
		// clamp(lerp, 0, 1)
		lerp = (lerp < 0.0) ? 0.0 : ((lerp > 1.0) ? 1.0 : lerp);

		// Get the vector from the point to the projection
		// (d * lerp) + a - pt
		double c0 = (dd[0] * lerp) + aa[0] - pt[0];
		double c1 = (dd[1] * lerp) + aa[1] - pt[1];

		// Square that length
		double l2 = (c0 *c0) + (c1 * c1);

		if (l2 < minVal) {
			minVal = l2;
			minIdx = i;
		}
	}
	return minIdx;
}



void handleMissing(
	const std::vector<uv_t> &uvs,
	const std::vector<edge_t> &borders,
	const std::vector<size_t> &missing,
	const std::vector<size_t> &borderToTri,
	std::vector<size_t> &triIdxs
) {
	float tol = 1.0f;

	// In the future, I will definitely need to do a
	// better search algorithm for this, but for now I can just handle it
	// with a brute force search

	std::vector<uv_t> starts;
	std::vector<uv_t> bDiff;
	std::vector<double> bLens2;
	bDiff.reserve(borders.size());
	bLens2.reserve(borders.size());
	starts.reserve(borders.size());
	for (size_t i = 0; i < borders.size(); ++i) {
		uv_t uv1 = uvs[borders[i][0]];
		uv_t uv2 = uvs[borders[i][1]];
		uv_t diff = { uv2[0] - uv1[0], uv2[1] - uv1[1] };
		double delta = diff[1] - diff[0];
		bLens2.push_back(delta * delta);
		bDiff.push_back(std::move(diff));
		starts.push_back(uv1);
	}

	for (auto &mIdx : missing) {
		uv_t mp = uvs[mIdx];
		size_t closestEdge = closestBruteForceEdge(starts, bDiff, bLens2, mp);
		size_t triIdx = borderToTri[closestEdge];
		triIdxs[mIdx] = triIdx;
	}
}

