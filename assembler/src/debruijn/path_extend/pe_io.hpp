/*
 * pe_io.hpp
 *
 *  Created on: Mar 15, 2012
 *      Author: andrey
 */

#ifndef PE_IO_HPP_
#define PE_IO_HPP_


#include "bidirectional_path.hpp"

namespace path_extend {

class ContigWriter {

protected:
    const Graph& g;

    size_t k_;

    std::string ToString(const BidirectionalPath& path) const {
        std::stringstream ss;

        if (!path.Empty()) {
            ss  << g.EdgeNucls(path[0]).Subseq(0, k_).str();
        }

        for (size_t i = 0; i < path.Size(); ++i) {
            if (path.GapAt(i) > (int) k_) {
                for (size_t j = 0; j < path.GapAt(i) - k_; ++ j) {
                    ss << "N";
                }
                ss << g.EdgeNucls(path[i]).str();
            }
            else {
                int overlapLen = k_ - path.GapAt(i);
                if (overlapLen >= (int) g.length(path[i]) + (int) k_) {
                    continue;
                }
                ss << g.EdgeNucls(path[i]).Subseq(overlapLen).str();
            }
        }

        return ss.str();
    }

    Sequence ToSequence(const BidirectionalPath& path) const {
        SequenceBuilder result;

        if (!path.Empty()) {
            result.append(g.EdgeNucls(path[0]).Subseq(0, k_));
        }
        for (size_t i = 0; i < path.Size(); ++i) {
            result.append(g.EdgeNucls(path[i]).Subseq(k_));
        }

        return result.BuildSequence();
    }


public:
    ContigWriter(const Graph& g_, size_t k): g(g_), k_(k) {

    }

    void writeEdges(const std::string& filename) {
        INFO("Outputting edges to " << filename);
        osequencestream_with_data_for_scaffold oss(filename);

        std::set<EdgeId> included;
        for (auto iter = g.SmartEdgeBegin(); !iter.IsEnd(); ++iter) {
            if (included.count(*iter) == 0) {
                oss.setCoverage(g.coverage(*iter));
                oss.setID(g.int_id(*iter));
                oss << g.EdgeNucls(*iter);

                included.insert(*iter);
                included.insert(g.conjugate(*iter));
            }
        }
        INFO("Contigs written");
    }


    void writePathEdges(const PathContainer& paths, const std::string& filename) {
		INFO("Outputting path data to " << filename);
		ofstream oss;
        oss.open(filename.c_str());
		for (size_t i = 0; i < paths.size(); ++i) {
            oss << i << endl;
            BidirectionalPath path = *paths.Get(i);
            oss << "PATH " << paths.Get(i)->GetId() << " " << path.Size() << " " << path.Length() + k_ << endl;
            for (size_t j = 0; j < path.Size(); ++j) {
			    oss << g.int_id(path[j]) << " " << g.length(path[j]) << endl;
            }
            oss << endl;
		}
		oss.close();
		INFO("Edges written");
	}

    void writePaths(PathContainer& paths, const std::string& filename) {

        INFO("Writing contigs to " << filename);
        osequencestream_with_data_for_scaffold oss(filename);

        for (auto iter = paths.begin(); iter != paths.end(); ++iter) {
            oss.setID(iter.get()->GetId());
            oss.setCoverage(iter.get()->Coverage());
            oss << ToString(*iter.get());
        }

        INFO("Contigs written");
    }

};


class PathInfoWriter {


public:

    void writePaths(PathContainer& paths, const std::string& filename) {
        ofstream oss(filename);

        for (auto iter = paths.begin(); iter != paths.end(); ++iter) {
            iter.get()->Print(oss);
        }

        oss.close();
    }
};

}

#endif /* PE_IO_HPP_ */