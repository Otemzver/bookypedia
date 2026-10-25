#include "use_cases_impl.h"

#include <stdexcept>

namespace app {

void UseCasesImpl::AddAuthor(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Author name must not be empty");
    }
    authors_.Save({domain::AuthorId::New(), name});
}

std::vector<domain::Author> UseCasesImpl::GetAuthors() {
    return authors_.GetAll();
}

void UseCasesImpl::AddBook(const std::string& author_id,
                           const std::string& title,
                           int publication_year) {
    if (title.empty()) {
        throw std::invalid_argument("Book title must not be empty");
    }
    Books().Save({domain::BookId::New(),
                  domain::AuthorId::FromString(author_id),
                  title,
                  publication_year});
}

std::vector<domain::Book> UseCasesImpl::GetBooks() {
    return Books().GetAll();
}

std::vector<domain::Book> UseCasesImpl::GetAuthorBooks(const std::string& author_id) {
    return Books().GetByAuthor(domain::AuthorId::FromString(author_id));
}

domain::BookRepository& UseCasesImpl::Books() {
    if (!books_) {
        throw std::logic_error("Book repository is not configured");
    }
    return *books_;
}

}  // namespace app
