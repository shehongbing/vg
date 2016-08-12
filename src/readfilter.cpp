#include "readfilter.hpp"
#include "IntervalTree.h"

#include <fstream>
#include <sstream>

namespace vg {

using namespace std;

bool ReadFilter::trim_ambiguous_ends(xg::XG* index, Alignment& alignment, int k) {
    assert(index != nullptr);
    assert(k < alignment.sequence().size());

    // Define a way to get node length, for flipping alignments
    function<int64_t(id_t)> get_node_length = [&index](id_t node) {
        return index->node_length(node);
    };

    // TODO: we're going to flip the alignment twice! This is a waste of time!
    // Define some kind of oriented view or something, or just two duplicated
    // trimming functions, so we can just trim once without flipping.

    // Trim the end
    bool end_changed = trim_ambiguous_end(index, alignment, k);
    // Flip and trim the start
    Alignment flipped = reverse_complement_alignment(alignment, get_node_length);
    
    if(trim_ambiguous_end(index, flipped, k)) {
        // The start needed trimming
        
        // Flip the trimmed flipped alignment back
        alignment = reverse_complement_alignment(flipped, get_node_length);
        // We definitely changed something
        return true;
    }
    
    // We maybe changed something
    return end_changed;
    
}

bool ReadFilter::trim_ambiguous_end(xg::XG* index, Alignment& alignment, int k) {
    // What mapping in the alignment is the leftmost one starting in the last k
    // bases? Start out with it set to the past-the-end value.
    size_t trim_start_mapping = alignment.path().mapping_size();
    // Where in the read sequence does it start?
    size_t trim_start_index = alignment.sequence().size();
    
    // How many real non-softclip bases have we seen reading in from the end of
    // the read?
    size_t real_base_count = 0;
    // How many softclip bases have we seen in from the end of the read?
    size_t softclip_base_count = 0;
    for(size_t i = alignment.path().mapping_size() - 1; i != -1; i--) {
        // Scan in from the end of the read.
        
        auto* mapping = alignment.mutable_path()->mutable_mapping(i);
        
        if(mapping->edit_size() == 0) {
            // Fix up no-edit mappings to perfect matches.
            // TODO: is there a utility function we should use instead?
            
            // The match length is the node length minus any offset.
            size_t match_length = index->node_length(mapping->position().node_id()) - mapping->position().offset();
            
            // Make and add the edit
            auto* edit = mapping->add_edit();
            edit->set_from_length(match_length);
            edit->set_to_length(match_length);
            
        }
        
        for(int j = mapping->edit_size() - 1; j != -1; j--) {
            // Visit every edit in the mapping
            auto& edit = mapping->edit(j);
            
            
            if(real_base_count == 0 && edit.from_length() == 0) {
                // This is a trailing insert. Put it as a softclip
                softclip_base_count += edit.to_length();
            } else {
                // This is some other kind of thing. Record it as real bases.
                real_base_count += edit.to_length();
            }
        }
        
        if(real_base_count <= k) {
            // This mapping starts fewer than k non-softclipped alignment
            // bases from the end of the read.
            trim_start_mapping = i;
            // Remember where in the string it actually starts.
            trim_start_index = alignment.sequence().size() - real_base_count - softclip_base_count;
        } else {
            // This mapping starts more than k in from the end. So the
            // previous one, if we had one, must be the right one.
            break;
        }
    }
    
    if(trim_start_mapping == alignment.path().mapping_size()) {
        // No mapping was found that starts within the last k non-softclipped
        // bases. So there's nothing to do.
        return false;
    }
    
    if(trim_start_mapping == 0) {
        // The very first mapping starts within the last k non-softclipped
        // bases. There's no previous unambiguous place we can go to anchor, so
        // we can't do the fancy search.
        return false;
    }
    
    if(real_base_count == 0) {
        // We have an anchoring mapping, but all the mappings we could trim are
        // softclips, so it's just an empty mapping. Don't do anything about it.
        return false;
    }
    
    // Which is the last assumed-non-ambiguous mapping from which we can anchor
    // our search?
    size_t root_mapping = trim_start_mapping - 1;
    
    // What's the sequence after that node that we are looking for? We need the
    // sequence for the mappings from the leftmost we might drop rightwards
    // until we get into softclips.
    string target_sequence = alignment.sequence().substr(trim_start_index,
        alignment.sequence().size() - trim_start_index - softclip_base_count);
        
    cerr << "Need to look for " << target_sequence << " right of mapping " << root_mapping << endl;
    
    // We're not going to recurse hundreds of nodes deep, so we can use the real
    // stack and a real recursive function.
    
    // Do the DFS into the given node, after already having matched the given
    // number of bases of the target sequence. See if you can match any more
    // bases of the target sequence.
    
    // Return the total number of leaves in all subtrees that match the full
    // target sequence, and the depth in bases of the shallowest point at which
    // multiple subtrees with full lenght matches are unified.
    auto do_dfs = [&](id_t node_id, bool is_reverse, size_t matched) -> pair<size_t, size_t> {
        // Grab the node sequence and match more of the target sequence. If we
        // finish the target sequence, return one match and unification at full
        // length (i.e. nothing can be discarded). If we mismatch, return 0
        // matches and unification at full length.
    
        // If we get through the whole node sequence without mismatching or
        // running out of target sequence, keep going.
    
        // Get all the places we can go to off of the right side of this
        // oriented node.
        
        // Recurse on all of them and collect the results.
        
        // Sum up the total leaf matches, which will be our leaf match count.
        
        // If multiple children have nonzero leaf match counts, report
        // unification at the end of this node. Otherwise, report unification at
        // the min unification depth of any subtree (and there will only be one
        // that isn't at full length).
        
        return make_pair(0, 0);
    };
    
    // TODO: call do_dfs(node, orientation, 0) on all the nodes right from the
    // root node. Do one final round of aggregation, and then we will know how
    // much of the target sequence we are allowed to keep and how much we need
    // to lose.
    
    
    
    // OVERALL STRATEGY
    
    // Look at the end of the read and find the first Mapping starting within k
    // bases of the end of the aligned region (accounting for softclips). If
    // there's none, it's not ambiguous.
    
    // Collect the sequence for that mapping to the end, not counting softclips.
    
    // Go one Mapping left of that mapping. If you can't, it's not ambiguous.
    
    // Do a depth-first search right from there. Every time you finish a
    // subtree, if more than one of the children of the subtree root contain
    // ways to spell out the end sequence, then you will need to clip back to
    // that subtree root or higher, so record the depth in bases to the end of
    // the subtree root.
    
    // Eventually all such subtrees will have an intersection with the subtree
    // that the actually aligned path lives in, so you will be able to guagantee
    // that the winning highest node is somewhere on the path actually taken.
    
    // Clip off any softclip and then trim back to that winning node rooting the
    // highest ambiguous subtree.
    
    // Repeat for the alignment in the other orientation (probably using a flip-
    // around function). (TODO: avoid flipping around all alignments in place?)
    
    return false;
}

bool ReadFilter::has_repeat(Alignment& aln, int k) {
    if (k == 0) {
        return false;
    }
    const string& s = aln.sequence();
    for (int i = 1; i <= 2 * k; ++i) {
        int covered = 0;
        bool ffound = true;
        bool bfound = true;
        for (int j = 1; (ffound || bfound) && (j + 1) * i < s.length(); ++j) {
            ffound = ffound && s.substr(0, i) == s.substr(j * i, i);
            bfound = bfound && s.substr(s.length() - i, i) == s.substr(s.length() - i - j * i, i);
            if (ffound || bfound) {
                covered += i;
            }
        }
        if (covered >= k) {
            return true;
        }
    }
    return false;
}

int ReadFilter::filter(istream* alignment_stream, xg::XG* xindex) {

    // name helper for output
    function<string(int)> chunk_name = [this](int num) -> string {
        stringstream ss;
        ss << outbase << "-" << num << ".gam";
        return ss.str();
    };

    // index regions by their inclusive ranges
    vector<Interval<int, int64_t> > interval_list;
    vector<Region> regions;
    // use strings instead of ofstreams because worried about too many handles
    vector<string> chunk_names;
    vector<bool> chunk_new; // flag if write or append

    // parse a bed, for now this is only way to do regions.  note
    // this operation converts from 0-based BED to 1-based inclusive VCF
    if (!regions_file.empty()) {
        if (outbase.empty()) {
            cerr << "-B option required with -R" << endl;
            return 1;
        }
        parse_bed_regions(regions_file, regions);
        if (regions.empty()) {
            cerr << "No regions read from BED file, doing whole graph" << endl;
        }
    }

    if(defray_length > 0 && xindex == nullptr) {
        cerr << "xg index required for end de-fraying" << endl;
        return 1;
    }
    
    if (regions.empty()) {
        // empty region, do everything
        // we handle empty intervals as special case when looking up, otherwise,
        // need to insert giant interval here.
        chunk_names.push_back(outbase.empty() ? "-" : chunk_name(0));
    } else {
        // otherwise, need to extract regions with xg
        if (xindex == nullptr) {
            cerr << "xg index required for -R option" << endl;
            return 1;
        }
    
        // fill in the map using xg index
        // relies entirely on the assumption that are path chunks
        // are perfectly spanned by an id range
        for (int i = 0; i < regions.size(); ++i) {
            Graph graph;
            int rank = xindex->path_rank(regions[i].seq);
            int path_size = rank == 0 ? 0 : xindex->path_length(regions[i].seq);

            if (regions[i].start >= path_size) {
                cerr << "Unable to find region in index: " << regions[i].seq << ":" << regions[i].start
                     << "-" << regions[i].end << endl;
            } else {
                // clip region on end of path
                regions[i].end = min(path_size - 1, regions[i].end);
                // do path node query
                // convert to 0-based coordinates as this seems to be what xg wants
                xindex->get_path_range(regions[i].seq, regions[i].start - 1, regions[i].end - 1, graph);
                if (context_size > 0) {
                    xindex->expand_context(graph, context_size);
                }
            }
            // find node range of graph, without bothering to build vg indices..
            int64_t min_id = numeric_limits<int64_t>::max();
            int64_t max_id = 0;
            for (int j = 0; j < graph.node_size(); ++j) {
                min_id = min(min_id, (int64_t)graph.node(j).id());
                max_id = max(max_id, (int64_t)graph.node(j).id());
            }
            // map the chunk id to a name
            chunk_names.push_back(chunk_name(i));

            // map the node range to the chunk id.
            if (graph.node_size() > 0) {
                interval_list.push_back(Interval<int, int64_t>(min_id, max_id, i));
                assert(chunk_names.size() == i + 1);
            }
        }
    }

    // index chunk regions
    IntervalTree<int, int64_t> region_map(interval_list);

    // which chunk(s) does a gam belong to?
    function<void(Alignment&, vector<int>&)> get_chunks = [&region_map, &regions](Alignment& aln, vector<int>& chunks) {
        // speed up case where no chunking
        if (regions.empty()) {
            chunks.push_back(0);
        } else {
            int64_t min_aln_id = numeric_limits<int64_t>::max();
            int64_t max_aln_id = -1;
            for (int i = 0; i < aln.path().mapping_size(); ++i) {
                const Mapping& mapping = aln.path().mapping(i);
                min_aln_id = min(min_aln_id, (int64_t)mapping.position().node_id());
                max_aln_id = max(max_aln_id, (int64_t)mapping.position().node_id());
            }
            vector<Interval<int, int64_t> > found_ranges;
            region_map.findOverlapping(min_aln_id, max_aln_id, found_ranges);
            for (auto& interval : found_ranges) {
                chunks.push_back(interval.value);
            }
        }
    };

    // buffered output (one buffer per chunk)
    vector<vector<Alignment> > buffer(chunk_names.size());
    int cur_buffer = -1;
    static const int buffer_size = 1000; // we let this be off by 1
    function<Alignment&(uint64_t)> write_buffer = [&buffer, &cur_buffer](uint64_t i) -> Alignment& {
        return buffer[cur_buffer][i];
    };
    // remember if write or append
    vector<bool> chunk_append(chunk_names.size(), false);

    // flush a buffer specified by cur_buffer to target in chunk_names, and clear it
    function<void()> flush_buffer = [&buffer, &cur_buffer, &write_buffer, &chunk_names, &chunk_append]() {
        ofstream outfile;
        auto& outbuf = chunk_names[cur_buffer] == "-" ? cout : outfile;
        if (chunk_names[cur_buffer] != "-") {
            outfile.open(chunk_names[cur_buffer], chunk_append[cur_buffer] ? ios::app : ios_base::out);
            chunk_append[cur_buffer] = true;
        }
        stream::write(outbuf, buffer[cur_buffer].size(), write_buffer);
        buffer[cur_buffer].clear();
    };

    // add alignment to all appropriate buffers, flushing as necessary
    // (note cur_buffer variable used here as a hack to specify which buffer is written to)
    function<void(Alignment&)> update_buffers = [&buffer, &cur_buffer, &region_map,
                                                 &write_buffer, &get_chunks, &flush_buffer](Alignment& aln) {
        vector<int> aln_chunks;
        get_chunks(aln, aln_chunks);
        for (auto chunk : aln_chunks) {
            buffer[chunk].push_back(aln);
            if (buffer[chunk].size() >= buffer_size) {
                // flush buffer
                cur_buffer = chunk;
                flush_buffer();
            }
        }
    };

    // keep track of how many reads were dropped by which option
    size_t pri_read_count = 0;
    size_t sec_read_count = 0;
    size_t sec_filtered_count = 0;
    size_t pri_filtered_count = 0;
    size_t min_sec_count = 0;
    size_t min_pri_count = 0;
    size_t min_sec_delta_count = 0;
    size_t min_pri_delta_count = 0;
    size_t max_sec_overhang_count = 0;
    size_t max_pri_overhang_count = 0;
    size_t min_sec_mapq_count = 0;
    size_t min_pri_mapq_count = 0;
    size_t repeat_sec_count = 0;
    size_t repeat_pri_count = 0;
    size_t defray_sec_count = 0;
    size_t defray_pri_count = 0;

    // for deltas, we keep track of last primary
    Alignment prev;
    bool prev_primary = false;
    bool keep_prev = true;
    double prev_score;

    // quick and dirty filter to see if removing reads that can slip around
    // and still map perfectly helps vg call.  returns true if at either
    // end of read sequence, at least k bases are repetitive, checking repeats
    // of up to size 2k
    
        
    // we assume that every primary alignment has 0 or 1 secondary alignment
    // immediately following in the stream
    function<void(Alignment&)> lambda = [&](Alignment& aln) {
        bool keep = true;
        double score = (double)aln.score();
        double denom = 2. * aln.sequence().length();
        // toggle substitution score
        if (sub_score == true) {
            // hack in ident to replace old counting logic.
            score = aln.identity() * aln.sequence().length();
            denom = aln.sequence().length();
            assert(score <= denom);
        }
        // toggle absolute or fractional score
        if (frac_score == true) {
            if (denom > 0.) {
                score /= denom;
            }
            else {
                assert(score == 0.);
            }
        }
        // compute overhang
        int overhang = 0;
        if (aln.path().mapping_size() > 0) {
            const auto& left_mapping = aln.path().mapping(0);
            if (left_mapping.edit_size() > 0) {
                overhang = left_mapping.edit(0).to_length() - left_mapping.edit(0).from_length();
            }
            const auto& right_mapping = aln.path().mapping(aln.path().mapping_size() - 1);
            if (right_mapping.edit_size() > 0) {
                const auto& edit = right_mapping.edit(right_mapping.edit_size() - 1);
                overhang = max(overhang, edit.to_length() - edit.from_length());
            }
        } else {
            overhang = aln.sequence().length();
        }

        if (aln.is_secondary()) {
            ++sec_read_count;
            assert(prev_primary && aln.name() == prev.name());
            double delta = prev_score - score;
            if (frac_delta == true) {
                delta = prev_score > 0 ? score / prev_score : 0.;
            }

            // filter (current) secondary
            keep = true;
            if (score < min_secondary) {
                ++min_sec_count;
                keep = false;
            }
            if ((keep || verbose) && delta < min_sec_delta) {
                ++min_sec_delta_count;
                keep = false;
            }
            if ((keep || verbose) && overhang > max_overhang) {
                ++max_sec_overhang_count;
                keep = false;
            }
            if ((keep || verbose) && aln.mapping_quality() < min_mapq) {
                ++min_sec_mapq_count;
                keep = false;
            }
            if ((keep || verbose) && has_repeat(aln, repeat_size)) {
                ++repeat_sec_count;
                keep = false;
            }
            if ((keep || verbose) && defray_length && trim_ambiguous_ends(xindex, aln, defray_length)) {
                ++defray_sec_count;
                // We keep these, because the alignments get modified.
            }
            if (!keep) {
                ++sec_filtered_count;
            }

            // filter unfiltered previous primary
            if (keep_prev && delta < min_pri_delta) {
                ++min_pri_delta_count;
                ++pri_filtered_count;
                keep_prev = false;
            }
            // add to write buffer
            if (keep) {
                update_buffers(aln);
            }
            if (keep_prev) {
                update_buffers(prev);
            }

            // forget last primary
            prev_primary = false;
            prev_score = -1;
            keep_prev = false;

        } else {
            // this awkward logic where we keep the primary and write in the secondary
            // is because we can only stream one at a time with for_each, but we need
            // to look at pairs (if secondary exists)...
            ++pri_read_count;
            if (keep_prev) {
                update_buffers(prev);
            }

            prev_primary = true;
            prev_score = score;
            keep_prev = true;
            if (score < min_primary) {
                ++min_pri_count;
                keep_prev = false;
            }
            if ((keep_prev || verbose) && overhang > max_overhang) {
                ++max_pri_overhang_count;
                keep_prev = false;
            }
            if ((keep_prev || verbose) && aln.mapping_quality() < min_mapq) {
                ++min_pri_mapq_count;
                keep_prev = false;
            }
            if ((keep_prev || verbose) && has_repeat(aln, repeat_size)) {
                ++repeat_pri_count;
                keep_prev = false;
            }
            if ((keep || verbose) && defray_length && trim_ambiguous_ends(xindex, aln, defray_length)) {
                ++defray_pri_count;
                // We keep these, because the alignments get modified.
            }
            if (!keep_prev) {
                ++pri_filtered_count;
            }
            // Copy after any modification
            prev = aln;
        }
    };
    stream::for_each(*alignment_stream, lambda);

    // flush buffer if trailing primary to keep
    if (keep_prev) {
        update_buffers(prev);
    }

    for (int i = 0; i < buffer.size(); ++i) {
        if (buffer[i].size() > 0) {
            cur_buffer = i;
            flush_buffer();
        }
    }

    if (verbose) {
        size_t tot_reads = pri_read_count + sec_read_count;
        size_t tot_filtered = pri_filtered_count + sec_filtered_count;
        cerr << "Total Filtered (primary):          " << pri_filtered_count << " / "
             << pri_read_count << endl
             << "Total Filtered (secondary):        " << sec_filtered_count << " / "
             << sec_read_count << endl
             << "Min Identity Filter (primary):     " << min_pri_count << endl
             << "Min Identity Filter (secondary):   " << min_sec_count << endl
             << "Min Delta Filter (primary):        " << min_pri_delta_count << endl
             << "Min Delta Filter (secondary):      " << min_sec_delta_count << endl
             << "Max Overhang Filter (primary):     " << max_pri_overhang_count << endl
             << "Max Overhang Filter (secondary):   " << max_sec_overhang_count << endl
             << "Repeat Ends Filter (primary):     " << repeat_pri_count << endl
             << "Repeat Ends Filter (secondary):   " << repeat_sec_count << endl

             << endl;
    }
    
    if (xindex != nullptr) {
        // Clean up any XG index we loaded.
        delete xindex;
    }

    return 0;

}

}
