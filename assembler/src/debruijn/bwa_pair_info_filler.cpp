//***************************************************************************
//* Copyright (c) 2015 Saint Petersburg State University
//* Copyright (c) 2011-2014 Saint Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//***************************************************************************

#include "bwa_pair_info_filler.hpp"

namespace bwa_pair_info {

BWAPairInfoFiller::EdgePair BWAPairInfoFiller::ConjugatePair(EdgePair ep) const {
    return make_pair(g_.conjugate(ep.second), g_.conjugate(ep.first));
}

void BWAPairInfoFiller::OutputEdges(const string &filename) const {
    io::osequencestream_simple oss(filename);
    for (auto it = g_.ConstEdgeBegin(true); !it.IsEnd(); ++it) {
        debruijn_graph::EdgeId e = *it;
        if (g_.length(e) >= min_contig_len_) {
            oss.set_header(ToString(g_.int_id(e)));
            oss << g_.EdgeNucls(e);
        }
    }
}
void BWAPairInfoFiller::FillEdgeIdMap() {
    for (auto it = g_.ConstEdgeBegin(true); !it.IsEnd(); ++it) {
        debruijn_graph::EdgeId e = *it;
        edge_id_map_.insert(make_pair(g_.int_id(e), e));
    }
}

bool BWAPairInfoFiller::CreateIndex(const string& contigs) {
    int run_res = 0;
    string err_log = path::append_path(work_dir_, "index.err");
    string index_line = bwa_path_ + string(" index ") + "-a is " + contigs + " 2>" + err_log;
    INFO("Running bwa index ... ");
    INFO("Command line: " << index_line);
    run_res = system(index_line.c_str());
    if (run_res != 0) {
        ERROR("bwa index failed, cannot align reads");
        return false;
    }
    return true;
}

bool BWAPairInfoFiller::CreateIndexIfNeeded(const string& contigs) {
    bool result = false;
    if (index_constructed_ || CreateIndex(contigs)) {
        index_constructed_ = true;
        index_base_ = contigs;
        result = true;
    }
    return result;
}

bool BWAPairInfoFiller::RunBWA(const string& reads_file, const string& out_sam_file) const {
    string run_command = bwa_path_ + " mem -t " + ToString(nthreads_) + " " + index_base_ + " "  + reads_file + "  > " + out_sam_file + " 2>"
        + out_sam_file + ".txt";
    INFO("Running bwa mem ...");
    INFO("Command line: " << run_command);

    int run_res = system(run_command.c_str());
    if (run_res != 0) {
        ERROR("bwa index failed, cannot align reads");
        return false;
    }
    return true;
}

bool BWAPairInfoFiller::AlignLib(const SequencingLibraryT& lib,
                                 const string& sam_file_base,
                                 vector<pair<string, string>>& resulting_sam_files) {

    VERIFY(index_constructed_);
    resulting_sam_files.clear();
    size_t file_index = 0;
    bool any_aligned = false;

    for (auto iter = lib.paired_begin(); iter != lib.paired_end(); iter++) {
        string left_reads = iter->first;
        string left_sam = sam_file_base + "_1_" + ToString(file_index) + ".sam";
        bool res = RunBWA(left_reads, left_sam);
        if (!res) {
            WARN("Failed to align left reads " << left_reads);
            continue;
        }
        string right_reads = iter->second;
        string right_sam = sam_file_base + "_2_" + ToString(file_index) + ".sam";
        res = RunBWA(right_reads, right_sam);
        if (!res) {
            WARN("Failed to align right reads " << right_reads);
            continue;
        }

        resulting_sam_files.push_back(make_pair(left_sam, right_sam));
        any_aligned = true;
    }
    return any_aligned;
}

int BWAPairInfoFiller::ProcessPairedRead(const SequencingLibraryT& lib,
                                          const MapperReadT& l, const MapperReadT& r,
                                          PairedInfoIndexT& paired_index) {
    using debruijn_graph::EdgeId;
    using io::LibraryOrientation;

    if (!l.IsValid() || !r.IsValid()) {
        return -1;
    }

    EdgeId e1 = edge_id_map_[stoi(l.get_contig_id())];
    EdgeId e2 = edge_id_map_[stoi(r.get_contig_id())];
    int l_pos = l.get_pos();
    int r_pos = r.get_pos();
    int r_from_pos_to_right_end = r.get_len() + r.get_right_hard_clip() - r.get_left_soft_clip();
    int l_from_pos_to_left_end = l.get_left_soft_clip() + l.get_left_hard_clip();

    if ((!l.is_forward() && (lib.orientation() == LibraryOrientation::FF || lib.orientation() == LibraryOrientation::FR)) ||
        (l.is_forward() && (lib.orientation() == LibraryOrientation::RF || lib.orientation() == LibraryOrientation::RR))) {
        e1 = g_.conjugate(e1);
        l_pos = (int) g_.length(e1) - l_pos - (l.get_len() - l.get_left_soft_clip() - l.get_right_soft_clip()) + (int) g_.k();
        l_from_pos_to_left_end = l.get_right_soft_clip() + l.get_right_hard_clip();
    }
    if ((!r.is_forward() && (lib.orientation() == LibraryOrientation::FF || lib.orientation() == LibraryOrientation::RF)) ||
        (r.is_forward() && (lib.orientation() == LibraryOrientation::FR || lib.orientation() == LibraryOrientation::RR))) {
        e2 = g_.conjugate(e2);
        r_pos = (int) g_.length(e2) - r_pos - (r.get_len() - r.get_left_soft_clip() - r.get_right_soft_clip()) + (int) g_.k();
        r_from_pos_to_right_end = r.get_len() + r.get_left_hard_clip() - r.get_right_soft_clip();
    }

    EdgePair ep{e1, e2};
    TRACE("Lpos " << l_pos << ", Rpos " << r_pos);
    TRACE("Ldist " << l_from_pos_to_left_end << ", Rdist " << r_from_pos_to_right_end)
    int edge_distance = (int) lib.data().mean_insert_size - l_from_pos_to_left_end - r_from_pos_to_right_end - r_pos + l_pos;
    TRACE("Distance " << edge_distance);

    if (ep > ConjugatePair(ep)) {
        ep = ConjugatePair(ep);
        edge_distance = edge_distance + (int) g_.length(e2) - (int) g_.length(e1);
        TRACE("New distance " << edge_distance);
    }

    paired_index.AddPairInfo(ep.first, ep.second, { (double) edge_distance, 1.0 });
    return edge_distance;
}

void BWAPairInfoFiller::ParseSAMFiles(const SequencingLibraryT& lib,
                                      const string& left_sam, const string& right_sam,
                                      PairedInfoIndexT& paired_index) {
    unordered_map<string, MapperReadT> left_reads;
    unordered_map<string, MapperReadT> right_reads;

    INFO("Reading SAM files " << left_sam << " and " << right_sam);
    MappedSamStream lf(left_sam);
    MappedSamStream rf(right_sam);
    while (!lf.eof() || !rf.eof()) {
        SingleSamRead left_read;
        MapperReadT left_data;
        string l_name = "";

        SingleSamRead right_read;
        MapperReadT right_data;
        string r_name = "";

        if (!lf.eof()) {
            lf >> left_read;
            l_name = left_read.get_name();
            if (left_read.is_properly_aligned()) {
                TRACE("Left read " << l_name);
                left_data = MapperReadT(string(lf.get_contig_name(left_read.get_contig_id())),
                                        left_read.get_pos(),
                                        left_read.get_data_len(),
                                        left_read.get_strand(),
                                        left_read.get_cigar());
            }
            else if (!left_read.is_main_alignment()) {
                TRACE("Ignoring left read");
                l_name = "";
            }
        }
        if (!rf.eof()) {
            rf >> right_read;
            r_name = right_read.get_name();
            if (right_read.is_properly_aligned()) {
                TRACE("Right read " << r_name);
                right_data = MapperReadT(string(rf.get_contig_name(right_read.get_contig_id())),
                                         right_read.get_pos(),
                                         right_read.get_data_len(),
                                         right_read.get_strand(),
                                         right_read.get_cigar());
            }
            else if (!right_read.is_main_alignment()) {
                TRACE("Ignoring right read");
                r_name = "";
            }
        }

        if (l_name == r_name) {
            TRACE("Equal processing");
            ProcessPairedRead(lib, left_data, right_data, paired_index);
                MapperReadT& ltmp = left_data;
                MapperReadT& rtmp = right_data;

                if ((ltmp.get_contig_id() == "113712" && rtmp.get_contig_id() == "109906") ||
                    (rtmp.get_contig_id() == "109906" && ltmp.get_contig_id() == "113712")) {

                    INFO("Connecting reads " << l_name);
                    INFO(ltmp.get_contig_id() << " -> " << rtmp.get_contig_id());
                    INFO(ltmp.get_pos() << " -> " << rtmp.get_pos());
                    INFO((ltmp.is_forward() ? "+" : "-")  << " -> " << (rtmp.is_forward() ? "+" : "-") );
                    INFO("(" << ltmp.get_left_hard_clip() << ", " << ltmp.get_right_hard_clip() << ") -> ("
                             << rtmp.get_left_hard_clip() << ", " << rtmp.get_right_hard_clip() << ")");
                    INFO("(" << ltmp.get_left_soft_clip() << ", " << ltmp.get_right_soft_clip() << ") -> ("
                             << rtmp.get_left_soft_clip() << ", " << rtmp.get_right_soft_clip() << ")");
                }
            continue;
        }

        if (r_name != "") {
            auto it = left_reads.find(r_name);
            if (it != left_reads.end())  {
                TRACE("Right read's mate found, processing");
                ProcessPairedRead(lib, it->second, right_data, paired_index);
                MapperReadT& ltmp = it->second;
                MapperReadT& rtmp = right_data;
                if ((ltmp.get_contig_id() == "113712" && rtmp.get_contig_id() == "109906") ||
                    (rtmp.get_contig_id() == "109906" && ltmp.get_contig_id() == "113712")) {

                    INFO("Connecting reads " << r_name);
                    INFO(ltmp.get_contig_id() << " -> " << rtmp.get_contig_id());
                    INFO(ltmp.get_pos() << " -> " << rtmp.get_pos());
                    INFO((ltmp.is_forward() ? "+" : "-")  << " -> " << (rtmp.is_forward() ? "+" : "-") );
                    INFO("(" << ltmp.get_left_hard_clip() << ", " << ltmp.get_right_hard_clip() << ") -> ("
                             << rtmp.get_left_hard_clip() << ", " << rtmp.get_right_hard_clip() << ")");
                    INFO("(" << ltmp.get_left_soft_clip() << ", " << ltmp.get_right_soft_clip() << ") -> ("
                             << rtmp.get_left_soft_clip() << ", " << rtmp.get_right_soft_clip() << ")");
                }

                left_reads.erase(it);
            }
            else {
                TRACE("Right read's mate not found, adding to map");
                if (right_reads.count(r_name) == 0) {
                    right_reads.emplace(r_name, right_data);
                } else {
                    WARN("Right read " << r_name << " is duplicated!");
                }
            }
        }

        if (l_name != "") {
            auto it = right_reads.find(l_name);
            if (it != right_reads.end()) {
                TRACE("Left read's mate found, processing");
                ProcessPairedRead(lib, left_data, it->second, paired_index);
                MapperReadT& ltmp = left_data;
                MapperReadT& rtmp = it->second;
                if ((ltmp.get_contig_id() == "113712" && rtmp.get_contig_id() == "109906") ||
                    (rtmp.get_contig_id() == "109906" && ltmp.get_contig_id() == "113712")) {

                    INFO("Connecting reads " << l_name);
                    INFO(ltmp.get_contig_id() << " -> " << rtmp.get_contig_id());
                    INFO(ltmp.get_pos() << " -> " << rtmp.get_pos());
                    INFO((ltmp.is_forward() ? "+" : "-")  << " -> " << (rtmp.is_forward() ? "+" : "-") );
                    INFO("(" << ltmp.get_left_hard_clip() << ", " << ltmp.get_right_hard_clip() << ") -> ("
                             << rtmp.get_left_hard_clip() << ", " << rtmp.get_right_hard_clip() << ")");
                    INFO("(" << ltmp.get_left_soft_clip() << ", " << ltmp.get_right_soft_clip() << ") -> ("
                             << rtmp.get_left_soft_clip() << ", " << rtmp.get_right_soft_clip() << ")");
                }

                right_reads.erase(it);
            }
            else {
                TRACE("Left read's mate not found, adding to map");
                if (left_reads.count(l_name) == 0) {
                    left_reads.emplace(l_name, left_data);
                } else {
                    WARN("Left read " << r_name << " is duplicated!");
                }

            }
        }
    }
}

bool BWAPairInfoFiller::Init() {
    INFO("Initializing bwa pair info counter, working dir " << work_dir_);
    path::make_dir(work_dir_);
    string edges_file = path::append_path(work_dir_, "long_edges.fasta");
    INFO("Saving edges to " << edges_file);
    OutputEdges(edges_file);
    FillEdgeIdMap();
    return CreateIndexIfNeeded(edges_file);
}

bool BWAPairInfoFiller::ProcessLib(size_t lib_index,
                                   const SequencingLibraryT& lib,
                                   PairedInfoIndexT& paired_index) {
    string lib_dir =  path::append_path(work_dir_, ToString(lib_index));
    path::make_dir(lib_dir);
    vector<pair<string, string>> sam_files;

    INFO("Processing lib #" << lib_index);
    if (!AlignLib(lib, path::append_path(lib_dir, "single"), sam_files)) {
        WARN("Failed to align lib #" << lib_index);
        return false;
    }

    for (auto it = g_.ConstEdgeBegin(); !it.IsEnd(); ++it)
        paired_index.AddPairInfo(*it, *it, { 0., 0. });

    INFO("Processing SAM files for lib #" << lib_index);
    for (auto sam_pair : sam_files) {
        ParseSAMFiles(lib, sam_pair.first, sam_pair.second, paired_index);
    }
    return true;
}

void BWAPairInfoFiller::MapperReadT::ParseCigar(const string& cigar) {
    string num = "";
    bool left_side = true;
    for (size_t i = 0; i < cigar.length(); ++i) {
        if (isdigit(cigar[i])) {
            num += cigar[i];
        }
        else {
            if (cigar[i] == 'H') {
                if (left_side)
                    left_hard_clip_ = (uint16_t) std::stoi(num);
                else
                    right_hard_clip_ = (uint16_t) std::stoi(num);
                num = "";
            }
            else if (cigar[i] == 'S') {
                if (left_side)
                    left_soft_clip_ = (uint16_t) std::stoi(num);
                else
                    right_soft_clip_ = (uint16_t) std::stoi(num);
                num = "";
            }
            else {
                left_side = false;
                num = "";
            }
        }
    }
}
}