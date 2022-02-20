#pragma once

#include <string>

namespace sbs {

/**
 * @brief A reference to a documentation page.
 *
 * Do not construct one directly. Use SBS_DOC_REF and SBS_ERROR_REF, which can be audited
 * automatically for spelling errors and staleness.
 */
struct e_doc_ref {
    std::string value;
};

}  // namespace sbs

#define SBS_DOC_REF(Page) (::sbs::e_doc_ref{::std::string(Page ".html")})
#define SBS_ERR_REF(Page) (::sbs::e_doc_ref{::std::string("err/" Page ".html")})
