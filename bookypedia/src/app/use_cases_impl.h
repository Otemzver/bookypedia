#pragma once

#include "../domain/author.h"
#include "../domain/book.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    // Этот конструктор нужен в том числе для unit-теста из precode.
    explicit UseCasesImpl(domain::AuthorRepository& authors)
        : authors_{authors} {
    }

    UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books)
        : authors_{authors}
        , books_{&books} {
    }

    void AddAuthor(const std::string& name) override;
    std::vector<domain::Author> GetAuthors() override;

    void AddBook(const std::string& author_id,
                 const std::string& title,
                 int publication_year) override;
    std::vector<domain::Book> GetBooks() override;
    std::vector<domain::Book> GetAuthorBooks(const std::string& author_id) override;

private:
    domain::BookRepository& Books();

    domain::AuthorRepository& authors_;
    domain::BookRepository* books_ = nullptr;
};

}  // namespace app
