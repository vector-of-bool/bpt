#include "./fs_transform.hpp"

#include <dds/error/errors.hpp>
#include <dds/util/fs.hpp>

#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/distance.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <semester/walk.hpp>

#include <nlohmann/json.hpp>

#include <iostream>

using namespace dds;

using require_obj   = semester::require_type<json5::data::mapping_type>;
using require_array = semester::require_type<json5::data::array_type>;
using require_str   = semester::require_type<std::string>;

dds::fs_transformation dds::fs_transformation::from_json(const json5::data& data) {
    fs_transformation ret;
    using namespace semester::walk_ops;

    auto prep_optional = [](auto& opt) {
        return [&](auto&&) {
            opt.emplace();
            return walk.pass;
        };
    };

    auto str_to_path = [](std::string const& s) {
        auto p = fs::path(s);
        if (p.is_absolute()) {
            throw semester::walk_error(std::string(walk.path())
                                       + ": Only relative paths are accepted");
        }
        return p;
    };

    auto get_strip_components = [](double d) {
        if (d != double(int(d)) || d < 0) {
            throw semester::walk_error(std::string(walk.path()) + ": "
                                       + "'strip-components' should be a positive whole number");
        }
        return int(d);
    };

    auto populate_globs = [&](std::vector<dds::glob>& globs) {
        return for_each{
            require_str{"Include/exclude list should be a list of globs"},
            put_into(std::back_inserter(globs),
                     [](const std::string& glob) {
                         try {
                             return dds::glob::compile(glob);
                         } catch (const std::runtime_error& e) {
                             throw semester::walk_error{std::string(walk.path()) + ": " + e.what()};
                         }
                     }),
        };
    };

    auto populate_reloc = [&](auto& op) {
        return [&](auto&& dat) {
            op.emplace();
            return mapping{
                required_key{"from",
                             "a 'from' path is required",
                             require_str{"'from' should be a path string"},
                             put_into(op->from, str_to_path)},
                required_key{"to",
                             "a 'to' path is required",
                             require_str{"'to' should be a path string"},
                             put_into(op->to, str_to_path)},
                if_key{"strip-components",
                       require_type<double>{"'strip-components' should be an integer"},
                       put_into(op->strip_components, get_strip_components)},
                if_key{"include",
                       require_array{"'include' should be an array"},
                       populate_globs(op->include)},
                if_key{"exclude",
                       require_array{"'exclude' should be an array"},
                       populate_globs(op->exclude)},
            }(dat);
        };
    };

    walk(data,
         require_obj{"Each transform must be a JSON object"},
         mapping{
             if_key{"copy", populate_reloc(ret.copy)},
             if_key{"move", populate_reloc(ret.move)},
             if_key{"remove",
                    require_obj{"'remove' should be a JSON object"},
                    prep_optional(ret.remove),
                    mapping{
                        required_key{"path",
                                     "'path' is required",
                                     require_str{"'path' should be a string path to remove"},
                                     put_into(ret.remove->path, str_to_path)},
                        if_key{"only-matching",
                               require_array{"'only-matching' should be an array of globs"},
                               populate_globs(ret.remove->only_matching)},
                    }},
             if_key{"write",
                    require_obj{"'write' should be a JSON object"},
                    prep_optional(ret.write),
                    mapping{
                        required_key{"path",
                                     "'path' is required",
                                     require_str{"'path' should be a string path to write to"},
                                     put_into(ret.write->path, str_to_path)},
                        required_key{"content",
                                     "'content' is required",
                                     require_str{"'content' must be a string"},
                                     put_into(ret.write->content)},
                    }},
         });

    return ret;
}

namespace {

bool matches_any(path_ref path, const std::vector<glob>& globs) {
    return std::any_of(globs.begin(), globs.end(), [&](auto&& gl) { return gl.match(path); });
}

bool parent_dir_of(fs::path root, fs::path child) {
    auto root_str  = (root += "/").lexically_normal().generic_string();
    auto child_str = (child += "/").lexically_normal().generic_string();
    return child_str.find(root_str) == 0;
}

void do_relocate(const dds::fs_transformation::copy_move_base& oper,
                 dds::path_ref                                 root,
                 bool                                          is_copy) {
    auto from = fs::weakly_canonical(root / oper.from);
    auto to   = fs::weakly_canonical(root / oper.to);
    if (!parent_dir_of(root, from)) {
        throw_external_error<errc::invalid_repo_transform>(
            "Filesystem transformation attempts to copy/move a file/directory from outside of the "
            "root [{}] into the root [{}].",
            from.string(),
            root.string());
    }
    if (!parent_dir_of(root, to)) {
        throw_external_error<errc::invalid_repo_transform>(
            "Filesystem transformation attempts to copy/move a file/directory [{}] to a "
            "destination outside of the restricted root [{}].",
            to.string(),
            root.string());
    }

    if (!fs::exists(from)) {
        throw_external_error<errc::invalid_repo_transform>(
            "Filesystem transformation attempting to copy/move a non-existint file/directory [{}] "
            "to [{}].",
            from.string(),
            to.string());
    }

    fs::create_directories(to.parent_path());

    if (fs::is_regular_file(from)) {
        if (is_copy) {
            fs::copy_file(from, to, fs::copy_options::overwrite_existing);
        } else {
            safe_rename(from, to);
        }
        return;
    }

    for (auto item : fs::recursive_directory_iterator(from)) {
        auto relpath      = fs::relative(item, from);
        auto matches_glob = [&](auto glob) { return glob.match(relpath.string()); };
        auto included     = oper.include.empty() || ranges::any_of(oper.include, matches_glob);
        auto excluded     = ranges::any_of(oper.exclude, matches_glob);
        if (!included || excluded) {
            continue;
        }

        auto n_components = ranges::distance(relpath);
        if (n_components <= oper.strip_components) {
            continue;
        }

        auto it = relpath.begin();
        std::advance(it, oper.strip_components);
        relpath = ranges::accumulate(it, relpath.end(), fs::path(), std::divides<>());

        auto dest = to / relpath;
        fs::create_directories(dest.parent_path());
        if (item.is_directory()) {
            fs::create_directories(dest);
        } else {
            if (is_copy) {
                fs::copy_file(item, dest, fs::copy_options::overwrite_existing);
            } else {
                safe_rename(item, dest);
            }
        }
    }
}

void do_remove(const struct fs_transformation::remove& oper, path_ref root) {
    auto from = fs::weakly_canonical(root / oper.path);
    if (!parent_dir_of(root, from)) {
        throw_external_error<errc::invalid_repo_transform>(
            "Filesystem transformation attempts to deletes files/directories outside of the "
            "root. Attempted to remove [{}]. Removal is restricted to [{}].",
            from.string(),
            root.string());
    }

    if (!fs::exists(from)) {
        throw_external_error<errc::invalid_repo_transform>(
            "Filesystem transformation attempts to delete a non-existint file/directory [{}].",
            from.string());
    }

    if (fs::is_directory(from)) {
        for (auto child : fs::recursive_directory_iterator{from}) {
            if (child.is_directory()) {
                continue;
            }
            if (!oper.only_matching.empty() && !matches_any(child, oper.only_matching)) {
                continue;
            }
            fs::remove_all(child);
        }
    } else {
        fs::remove_all(from);
    }
}

void do_write(const struct fs_transformation::write& oper, path_ref root) {
    auto dest = fs::weakly_canonical(root / oper.path);
    if (!parent_dir_of(root, dest)) {
        throw_external_error<errc::invalid_repo_transform>(
            "Filesystem transformation is trying to write outside of the root. Attempted to write "
            "to [{}]. Writing is restricted to [{}].",
            dest.string(),
            root.string());
    }

    std::cout << "Write content: " << oper.content;

    auto of = dds::open(dest, std::ios::binary | std::ios::out);
    of << oper.content;
}

}  // namespace

void dds::fs_transformation::apply_to(dds::path_ref root) const {
    if (copy) {
        do_relocate(*copy, root, true);
    }
    if (move) {
        do_relocate(*move, root, false);
    }
    if (remove) {
        do_remove(*remove, root);
    }
    if (write) {
        do_write(*write, root);
    }
}

namespace {

nlohmann::json reloc_as_json(const fs_transformation::copy_move_base& oper) {
    auto obj    = nlohmann::json::object();
    obj["from"] = oper.from.string();
    obj["to"]   = oper.to.string();

    obj["strip-components"] = oper.strip_components;

    auto inc_list = nlohmann::json::array();
    for (auto& inc : oper.include) {
        inc_list.push_back(inc.string());
    }

    auto exc_list = nlohmann::json::array();
    for (auto& exc : oper.exclude) {
        exc_list.push_back(exc.string());
    }

    if (!inc_list.empty()) {
        obj["include"] = inc_list;
    }
    if (!exc_list.empty()) {
        obj["exclude"] = exc_list;
    }

    return obj;
}

}  // namespace

std::string fs_transformation::as_json() const noexcept {
    auto obj = nlohmann::json::object();
    if (copy) {
        obj["copy"] = reloc_as_json(*copy);
    }
    if (move) {
        obj["move"] = reloc_as_json(*move);
    }
    if (remove) {
        auto rm    = nlohmann::json::object();
        rm["path"] = remove->path.string();
        if (!remove->only_matching.empty()) {
            auto if_arr = nlohmann::json::array();
            for (auto&& gl : remove->only_matching) {
                if_arr.push_back(gl.string());
            }
            rm["only-matching"] = rm;
        }
        obj["remove"] = rm;
    }
    if (write) {
        auto wr       = nlohmann::json::object();
        wr["path"]    = write->path.string();
        wr["content"] = write->content;
        obj["write"]  = wr;
    }

    return to_string(obj);
}
