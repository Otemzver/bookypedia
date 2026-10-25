#pragma once

#include <pqxx/connection>
#include <optional>
#include <string>
#include <vector>

#include "../domain/author.h"
#include "../domain/book.h"

namespace postgres {

struct BookInfo {
    domain::BookId id;
    domain::AuthorId author_id;
    std::string title;
    std::string author;
    int publication_year = 0;
    std::vector<std::string> tags;
};

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection) : connection_{connection} {}
    void Save(const domain::Author& author) override;
    std::vector<domain::Author> GetAll() override;
private:
    pqxx::connection& connection_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection) : connection_{connection} {}
    void Save(const domain::Book& book) override;
    std::vector<domain::Book> GetAll() override;
    std::vector<domain::Book> GetByAuthor(const domain::AuthorId& author_id) override;
private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    AuthorRepositoryImpl& GetAuthors() & { return authors_; }
    BookRepositoryImpl& GetBooks() & { return books_; }

    std::vector<domain::Author> LoadAuthors();
    std::optional<domain::Author> FindAuthorByName(const std::string& name);

    // One public mutating operation == one DB transaction.
    bool AddAuthor(const std::string& name);
    bool DeleteAuthorById(const domain::AuthorId& id);
    bool EditAuthorById(const domain::AuthorId& id, const std::string& new_name);

    bool AddBook(const domain::AuthorId& author_id, const std::string& title, int year,
                 const std::vector<std::string>& tags);
    bool AddBookWithNewAuthor(const std::string& author_name, const std::string& title, int year,
                              const std::vector<std::string>& tags);

    std::vector<BookInfo> LoadBooks();
    std::vector<BookInfo> FindBooksByTitle(const std::string& title);
    std::optional<BookInfo> FindBookById(const domain::BookId& id);

    bool DeleteBook(const domain::BookId& id);
    bool EditBook(const domain::BookId& id, const std::string& title, int year,
                  const std::vector<std::string>& tags);

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
};

}  // namespace postgres
