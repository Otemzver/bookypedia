#include "view.h"
#include <istream>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <functional>
#include <set>
#include <sstream>
#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {
namespace {
std::string Rest(std::istream& in) {
    std::string s;
    std::getline(in, s);
    boost::algorithm::trim(s);
    return s;
}

std::vector<std::string> NormalizeTags(std::string line) {
    std::vector<std::string> raw;
    boost::split(raw, line, boost::is_any_of(","));
    std::set<std::string> unique;
    for (auto& tag : raw) {
        boost::algorithm::trim(tag);
        std::vector<std::string> words;
        boost::split(words, tag, boost::is_space(), boost::token_compress_on);
        std::string normalized;
        for (auto& word : words) {
            if (word.empty()) continue;
            if (!normalized.empty()) normalized += ' ';
            normalized += word;
        }
        if (!normalized.empty() && normalized.size() <= 30) unique.insert(std::move(normalized));
    }
    return {unique.begin(), unique.end()};
}
}

View::View(menu::Menu& menu, postgres::Database& db, std::istream& input, std::ostream& output)
    : menu_{menu}, db_{db}, input_{input}, output_{output} {
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s, std::bind(&View::AddAuthor,this,ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors,this));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s, std::bind(&View::AddBook,this,ph::_1));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks,this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s, std::bind(&View::ShowAuthorBooks,this));
    menu_.AddAction("DeleteAuthor"s, "[name]"s, "Deletes author"s, std::bind(&View::DeleteAuthor,this,ph::_1));
    menu_.AddAction("EditAuthor"s, "[name]"s, "Edits author"s, std::bind(&View::EditAuthor,this,ph::_1));
    menu_.AddAction("ShowBook"s, "[title]"s, "Shows book"s, std::bind(&View::ShowBook,this,ph::_1));
    menu_.AddAction("DeleteBook"s, "[title]"s, "Deletes book"s, std::bind(&View::DeleteBook,this,ph::_1));
    menu_.AddAction("EditBook"s, "[title]"s, "Edits book"s, std::bind(&View::EditBook,this,ph::_1));
}

void View::PrintAuthors(const std::vector<domain::Author>& authors) const {
    for (size_t i=0;i<authors.size();++i) output_ << i+1 << " " << authors[i].GetName() << '\n';
}
void View::PrintBooks(const std::vector<postgres::BookInfo>& books) const {
    for (size_t i=0;i<books.size();++i)
        output_ << i+1 << " " << books[i].title << " by " << books[i].author
                << ", " << books[i].publication_year << '\n';
}
void View::PrintBook(const postgres::BookInfo& b) const {
    output_ << "Title: " << b.title << '\n'
            << "Author: " << b.author << '\n'
            << "Publication year: " << b.publication_year << '\n';
    if (!b.tags.empty()) {
        output_ << "Tags: ";
        for (size_t i=0;i<b.tags.size();++i) {
            if (i) output_ << ", ";
            output_ << b.tags[i];
        }
        output_ << '\n';
    }
}

bool View::AddAuthor(std::istream& cmd) const {
    auto name=Rest(cmd);
    if (name.empty() || !db_.AddAuthor(name)) output_ << "Failed to add author\n";
    return true;
}
bool View::ShowAuthors() const { PrintAuthors(db_.LoadAuthors()); return true; }

std::optional<domain::Author> View::SelectAuthor(bool print_select) const {
    auto authors=db_.LoadAuthors();
    if (print_select) output_ << "Select author:\n";
    PrintAuthors(authors);
    output_ << "Enter author # or empty line to cancel\n";
    std::string line;
    if (!std::getline(input_,line) || line.empty()) return std::nullopt;
    try {
        size_t pos=0; int n=std::stoi(line,&pos);
        if (pos!=line.size() || n<1 || static_cast<size_t>(n)>authors.size()) return std::nullopt;
        return authors[n-1];
    } catch (...) { return std::nullopt; }
}
std::optional<domain::Author> View::ResolveAuthor(std::string name, bool print_select) const {
    boost::algorithm::trim(name);
    if (name.empty()) return SelectAuthor(print_select);
    return db_.FindAuthorByName(name);
}

std::vector<std::string> View::ReadTags() const {
    output_ << "Enter tags (comma separated):\n";
    std::string line; std::getline(input_,line);
    return NormalizeTags(line);
}

bool View::AddBook(std::istream& cmd) const {
    int year=0;
    if (!(cmd>>year)) { output_<<"Failed to add book\n"; return true; }
    auto title=Rest(cmd);
    if (title.empty()) { output_<<"Failed to add book\n"; return true; }

    output_ << "Enter author name or empty line to select from list:\n";
    std::string author_name; std::getline(input_,author_name);
    boost::algorithm::trim(author_name);

    if (author_name.empty()) {
        auto author=SelectAuthor(true);
        if (!author) { ReadTags(); output_<<"Failed to add book\n"; return true; }
        auto tags=ReadTags();
        if (!db_.AddBook(author->GetId(),title,year,tags)) output_<<"Failed to add book\n";
        return true;
    }

    if (auto author=db_.FindAuthorByName(author_name)) {
        auto tags=ReadTags();
        if (!db_.AddBook(author->GetId(),title,year,tags)) output_<<"Failed to add book\n";
        return true;
    }

    output_ << "No author found. Do you want to add " << author_name << " (y/n)?\n";
    std::string answer; std::getline(input_,answer);
    if (!(answer=="y" || answer=="Y")) { output_<<"Failed to add book\n"; return true; }
    auto tags=ReadTags();
    if (!db_.AddBookWithNewAuthor(author_name,title,year,tags)) output_<<"Failed to add book\n";
    return true;
}

bool View::ShowBooks() const { PrintBooks(db_.LoadBooks()); return true; }

bool View::ShowAuthorBooks() const {
    auto author=SelectAuthor(true);
    if (!author) return true;
    std::vector<postgres::BookInfo> books;
    for (auto& b:db_.LoadBooks()) if (b.author_id==author->GetId()) books.push_back(std::move(b));
    for (size_t i = 0; i < books.size(); ++i)
        output_ << i + 1 << " " << books[i].title << ", " << books[i].publication_year << '\n';
    return true;
}

bool View::DeleteAuthor(std::istream& cmd) const {
    auto name=Rest(cmd);
    std::optional<domain::Author> author;
    if (name.empty()) author=SelectAuthor(true);
    else author=db_.FindAuthorByName(name);
    if (!author || !db_.DeleteAuthorById(author->GetId())) output_<<"Failed to delete author\n";
    return true;
}

bool View::EditAuthor(std::istream& cmd) const {
    auto name=Rest(cmd);
    std::optional<domain::Author> author;
    if (name.empty()) author=SelectAuthor(true);
    else author=db_.FindAuthorByName(name);
    if (!author) { output_<<"Failed to edit author\n"; return true; }
    output_<<"Enter new name:\n";
    std::string new_name; std::getline(input_,new_name); boost::algorithm::trim(new_name);
    if (new_name.empty() || !db_.EditAuthorById(author->GetId(),new_name))
        output_<<"Failed to edit author\n";
    return true;
}

std::optional<postgres::BookInfo> View::SelectBook(std::string title) const {
    boost::algorithm::trim(title);
    auto books = title.empty() ? db_.LoadBooks() : db_.FindBooksByTitle(title);
    if (books.empty()) return std::nullopt;
    if (!title.empty() && books.size()==1) return books.front();
    PrintBooks(books);
    output_<<"Enter the book # or empty line to cancel:\n";
    std::string line; if (!std::getline(input_,line) || line.empty()) return std::nullopt;
    try {
        size_t pos=0; int n=std::stoi(line,&pos);
        if (pos!=line.size() || n<1 || static_cast<size_t>(n)>books.size()) return std::nullopt;
        return books[n-1];
    } catch (...) { return std::nullopt; }
}

bool View::ShowBook(std::istream& cmd) const {
    auto title=Rest(cmd);
    auto book=SelectBook(title);
    if (book) PrintBook(*book);
    return true;
}
bool View::DeleteBook(std::istream& cmd) const {
    auto book=SelectBook(Rest(cmd));
    if (book && !db_.DeleteBook(book->id)) output_<<"Failed to delete book\n";
    return true;
}
bool View::EditBook(std::istream& cmd) const {
    auto book=SelectBook(Rest(cmd));
    if (!book) {
        // Cancellation is silent; an explicitly named absent book is an error.
        // Determine whether a title was supplied is no longer available here, so caller's
        // command stream is consumed above. For black-box semantics, no selection means not found.
        output_<<"Book not found\n";
        return true;
    }
    // Re-check immediately before editing to detect concurrent deletion.
    auto fresh=db_.FindBookById(book->id);
    if (!fresh) { output_<<"Book not found\n"; return true; }
    book=*fresh;

    output_<<"Enter new title or empty line to use the current one ("<<book->title<<"):\n";
    std::string title; std::getline(input_,title); boost::algorithm::trim(title);
    if (title.empty()) title=book->title;

    output_<<"Enter publication year or empty line to use the current one ("<<book->publication_year<<"):\n";
    std::string year_line; std::getline(input_,year_line); boost::algorithm::trim(year_line);
    int year=book->publication_year;
    if (!year_line.empty()) {
        try { size_t p=0; year=std::stoi(year_line,&p); if(p!=year_line.size()) throw std::runtime_error("x"); }
        catch (...) { output_<<"Book not found\n"; return true; }
    }

    output_<<"Enter tags (current tags: ";
    for(size_t i=0;i<book->tags.size();++i){ if(i) output_<<", "; output_<<book->tags[i]; }
    output_<<"):\n";
    std::string tag_line; std::getline(input_,tag_line);
    auto tags=NormalizeTags(tag_line);

    if (!db_.EditBook(book->id,title,year,tags)) output_<<"Book not found\n";
    return true;
}

}  // namespace ui
