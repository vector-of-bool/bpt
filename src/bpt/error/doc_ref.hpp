#pragma once

#include <string>

namespace bpt {

/**
 * @brief A reference to a documentation page.
 *
 * Do not construct one directly. Use BPT_DOC_REF and BPT_ERROR_REF, which can be audited
 * automatically for spelling errors and staleness.
 */
struct e_doc_ref {
    std::string value;
};

}  // namespace bpt

#define BPT_DOC_REF(Page) (::bpt::e_doc_ref{::std::string(Page ".html")})
#define BPT_ERR_REF(Page) (::bpt::e_doc_ref{::std::string("err/" Page ".html")})
