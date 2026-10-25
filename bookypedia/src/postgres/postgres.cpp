#include "postgres.h"

#include <pqxx/pqxx>
#include <pqxx/zview.hxx>

namespace postgres {
using pqxx::operator"" _zv;

namespace {
domain::Author ReadAuthor(const pqxx::row& row) {
    return {domain::AuthorId::FromString(row["id"].as<std::string>()),
            row["name"].as<std::string>()};
}

BookInfo ReadBook(const pqxx::row& row) {
    return {domain::BookId::FromString(row["id"].as<std::string>()),
            domain::AuthorId::FromString(row["author_id"].as<std::string>()),
            row["title"].as<std::string>(),
            row["author"].as<std::string>(),
            row["publication_year"].as<int>(),
            {}};
}

void InsertTags(pqxx::work& tx, const domain::BookId& book_id,
                const std::vector<std::string>& tags) {
    for (const auto& tag : tags) {
        tx.exec_params("INSERT INTO book_tags (book_id, tag) VALUES ($1, $2)"_zv,
                       book_id.ToString(), tag);
    }
}

std::vector<std::string> LoadTags(pqxx::transaction_base& tx, const domain::BookId& book_id) {
    const auto rows = tx.exec_params(
        "SELECT tag FROM book_tags WHERE book_id=$1 ORDER BY tag ASC"_zv,
        book_id.ToString());
    std::vector<std::string> tags;
    tags.reserve(rows.size());
    for (const auto& row : rows) tags.push_back(row[0].as<std::string>());
    return tags;
}

std::vector<BookInfo> ReadBooks(pqxx::transaction_base& tx, const pqxx::result& rows) {
    std::vector<BookInfo> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        auto book = ReadBook(row);
        book.tags = LoadTags(tx, book.id);
        result.push_back(std::move(book));
    }
    return result;
}
}  // namespace

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work tx{connection_};
    tx.exec_params("INSERT INTO authors (id,name) VALUES ($1,$2)"_zv,
                   author.GetId().ToString(), author.GetName());
    tx.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAll() {
    pqxx::read_transaction tx{connection_};
    const auto rows = tx.exec("SELECT id,name FROM authors ORDER BY name ASC"_zv);
    std::vector<domain::Author> result;
    for (const auto& row : rows) result.push_back(ReadAuthor(row));
    return result;
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work tx{connection_};
    tx.exec_params(
        "INSERT INTO books (id,author_id,title,publication_year) VALUES ($1,$2,$3,$4)"_zv,
        book.GetId().ToString(), book.GetAuthorId().ToString(),
        book.GetTitle(), book.GetPublicationYear());
    tx.commit();
}

std::vector<domain::Book> BookRepositoryImpl::GetAll() {
    pqxx::read_transaction tx{connection_};
    const auto rows = tx.exec(
        "SELECT id,author_id,title,publication_year FROM books ORDER BY title ASC"_zv);
    std::vector<domain::Book> result;
    for (const auto& row : rows) {
        result.emplace_back(domain::BookId::FromString(row["id"].as<std::string>()),
                            domain::AuthorId::FromString(row["author_id"].as<std::string>()),
                            row["title"].as<std::string>(),
                            row["publication_year"].as<int>());
    }
    return result;
}

std::vector<domain::Book> BookRepositoryImpl::GetByAuthor(const domain::AuthorId& author_id) {
    pqxx::read_transaction tx{connection_};
    const auto rows = tx.exec_params(
        "SELECT id,author_id,title,publication_year FROM books WHERE author_id=$1 "
        "ORDER BY publication_year ASC,title ASC"_zv, author_id.ToString());
    std::vector<domain::Book> result;
    for (const auto& row : rows) {
        result.emplace_back(domain::BookId::FromString(row["id"].as<std::string>()),
                            domain::AuthorId::FromString(row["author_id"].as<std::string>()),
                            row["title"].as<std::string>(),
                            row["publication_year"].as<int>());
    }
    return result;
}

Database::Database(pqxx::connection connection) : connection_{std::move(connection)} {
    pqxx::work tx{connection_};
    tx.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);
    tx.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID PRIMARY KEY,
    author_id UUID NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
    title varchar(100) NOT NULL,
    publication_year integer NOT NULL
);
)"_zv);
    tx.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    tag varchar(30) NOT NULL,
    PRIMARY KEY (book_id, tag)
);
)"_zv);
    tx.commit();
}

std::vector<domain::Author> Database::LoadAuthors() {
    pqxx::read_transaction tx{connection_};
    const auto rows = tx.exec("SELECT id,name FROM authors ORDER BY name ASC"_zv);
    std::vector<domain::Author> result;
    for (const auto& row : rows) result.push_back(ReadAuthor(row));
    return result;
}

std::optional<domain::Author> Database::FindAuthorByName(const std::string& name) {
    pqxx::read_transaction tx{connection_};
    const auto rows = tx.exec_params("SELECT id,name FROM authors WHERE name=$1"_zv, name);
    if (rows.empty()) return std::nullopt;
    return ReadAuthor(rows[0]);
}

bool Database::AddAuthor(const std::string& name) {
    try {
        pqxx::work tx{connection_};
        tx.exec_params("INSERT INTO authors(id,name) VALUES($1,$2)"_zv,
                       domain::AuthorId::New().ToString(), name);
        tx.commit();
        return true;
    } catch (const pqxx::sql_error&) { return false; }
}

bool Database::DeleteAuthorById(const domain::AuthorId& id) {
    try {
        pqxx::work tx{connection_};
        tx.exec_params("DELETE FROM book_tags WHERE book_id IN (SELECT id FROM books WHERE author_id=$1)"_zv, id.ToString());
        tx.exec_params("DELETE FROM books WHERE author_id=$1"_zv, id.ToString());
        auto r = tx.exec_params("DELETE FROM authors WHERE id=$1"_zv, id.ToString());
        if (r.affected_rows() != 1) return false;
        tx.commit();
        return true;
    } catch (const pqxx::sql_error&) { return false; }
}

bool Database::EditAuthorById(const domain::AuthorId& id, const std::string& new_name) {
    try {
        pqxx::work tx{connection_};
        auto r = tx.exec_params("UPDATE authors SET name=$1 WHERE id=$2"_zv,
                                new_name, id.ToString());
        if (r.affected_rows() != 1) return false;
        tx.commit();
        return true;
    } catch (const pqxx::sql_error&) { return false; }
}

bool Database::AddBook(const domain::AuthorId& author_id, const std::string& title, int year,
                       const std::vector<std::string>& tags) {
    try {
        pqxx::work tx{connection_};
        const auto id = domain::BookId::New();
        tx.exec_params(
            "INSERT INTO books(id,author_id,title,publication_year) VALUES($1,$2,$3,$4)"_zv,
            id.ToString(), author_id.ToString(), title, year);
        InsertTags(tx, id, tags);
        tx.commit();
        return true;
    } catch (const pqxx::sql_error&) { return false; }
}

bool Database::AddBookWithNewAuthor(const std::string& author_name, const std::string& title,
                                    int year, const std::vector<std::string>& tags) {
    try {
        pqxx::work tx{connection_};
        const auto author_id = domain::AuthorId::New();
        const auto book_id = domain::BookId::New();
        tx.exec_params("INSERT INTO authors(id,name) VALUES($1,$2)"_zv,
                       author_id.ToString(), author_name);
        tx.exec_params(
            "INSERT INTO books(id,author_id,title,publication_year) VALUES($1,$2,$3,$4)"_zv,
            book_id.ToString(), author_id.ToString(), title, year);
        InsertTags(tx, book_id, tags);
        tx.commit();
        return true;
    } catch (const pqxx::sql_error&) { return false; }
}

std::vector<BookInfo> Database::LoadBooks() {
    pqxx::read_transaction tx{connection_};
    auto rows = tx.exec(R"(
SELECT b.id,b.author_id,b.title,a.name AS author,b.publication_year
FROM books b JOIN authors a ON a.id=b.author_id
ORDER BY b.title ASC,a.name ASC,b.publication_year ASC
)"_zv);
    return ReadBooks(tx, rows);
}

std::vector<BookInfo> Database::FindBooksByTitle(const std::string& title) {
    pqxx::read_transaction tx{connection_};
    auto rows = tx.exec_params(R"(
SELECT b.id,b.author_id,b.title,a.name AS author,b.publication_year
FROM books b JOIN authors a ON a.id=b.author_id
WHERE b.title=$1
ORDER BY b.title ASC,a.name ASC,b.publication_year ASC
)"_zv, title);
    return ReadBooks(tx, rows);
}

std::optional<BookInfo> Database::FindBookById(const domain::BookId& id) {
    pqxx::read_transaction tx{connection_};
    auto rows = tx.exec_params(R"(
SELECT b.id,b.author_id,b.title,a.name AS author,b.publication_year
FROM books b JOIN authors a ON a.id=b.author_id WHERE b.id=$1
)"_zv, id.ToString());
    if (rows.empty()) return std::nullopt;
    auto result = ReadBook(rows[0]);
    result.tags = LoadTags(tx, result.id);
    return result;
}

bool Database::DeleteBook(const domain::BookId& id) {
    try {
        pqxx::work tx{connection_};
        tx.exec_params("DELETE FROM book_tags WHERE book_id=$1"_zv, id.ToString());
        auto r = tx.exec_params("DELETE FROM books WHERE id=$1"_zv, id.ToString());
        if (r.affected_rows() != 1) return false;
        tx.commit();
        return true;
    } catch (const pqxx::sql_error&) { return false; }
}

bool Database::EditBook(const domain::BookId& id, const std::string& title, int year,
                        const std::vector<std::string>& tags) {
    try {
        pqxx::work tx{connection_};
        auto r = tx.exec_params(
            "UPDATE books SET title=$1,publication_year=$2 WHERE id=$3"_zv,
            title, year, id.ToString());
        if (r.affected_rows() != 1) return false;
        tx.exec_params("DELETE FROM book_tags WHERE book_id=$1"_zv, id.ToString());
        InsertTags(tx, id, tags);
        tx.commit();
        return true;
    } catch (const pqxx::sql_error&) { return false; }
}

}  // namespace postgres
