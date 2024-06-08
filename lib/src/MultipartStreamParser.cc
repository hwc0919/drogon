/**
 *
 *  @file MultipartStreamParser.h
 *  @author Nitromelon
 *
 *  Copyright 2024, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "MultipartStreamParser.h"

drogon::MultipartStreamParser::MultipartStreamParser(
    const std::string &contentType)
{
    std::string::size_type pos = contentType.find(';');
    if (pos == std::string::npos)
    {
        isValid_ = false;
        return;
    }

    std::string type = contentType.substr(0, pos);
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) {
        return tolower(c);
    });
    if (type != "multipart/form-data")
    {
        isValid_ = false;
        return;
    }
    pos = contentType.find("boundary=");
    if (pos == std::string::npos)
    {
        isValid_ = false;
        return;
    }
    auto pos2 = contentType.find(';', pos);
    if (pos2 == std::string::npos)
        pos2 = contentType.size();

    boundary_ = contentType.substr(pos + 9, pos2 - (pos + 9));
    dashBoundaryCrlf_ = dash_ + boundary_ + crlf_;
    crlfDashBoundary_ = crlf_ + dash_ + boundary_;
}

static bool startsWith(const std::string_view &a, const std::string_view &b)
{
    if (a.size() < b.size())
    {
        return false;
    }
    for (size_t i = 0; i < b.size(); i++)
    {
        if (a[i] != b[i])
        {
            return false;
        }
    }
    return true;
}

static bool startsWithIgnoreCase(const std::string_view &a,
                                 const std::string_view &b)
{
    if (a.size() < b.size())
    {
        return false;
    }
    for (size_t i = 0; i < b.size(); i++)
    {
        if (::tolower(a[i]) != ::tolower(b[i]))
        {
            return false;
        }
    }
    return true;
}

// TODO: same function in HttpRequestParser.cc
static std::pair<std::string_view, std::string_view> parseLine(
    const char *begin,
    const char *end)
{
    auto p = begin;
    while (p != end)
    {
        if (*p == ':')
        {
            if (p + 1 != end && *(p + 1) == ' ')
            {
                return std::make_pair(std::string_view(begin, p - begin),
                                      std::string_view(p + 2, end - p - 2));
            }
            else
            {
                return std::make_pair(std::string_view(begin, p - begin),
                                      std::string_view(p + 1, end - p - 1));
            }
        }
        ++p;
    }
    return std::make_pair(std::string_view(), std::string_view());
}

void drogon::MultipartStreamParser::parse(
    const char *data,
    size_t length,
    const drogon::HttpStreamHandler::MultipartHeaderCallback &headerCb,
    const drogon::HttpStreamHandler::StreamDataCallback &dataCb)
{
    buffer_.append(data, length);

    while (buffer_.readableBytes() > 0)
    {
        switch (status_)
        {
            case Status::kExpectFirstBoundary:
            {
                std::string_view buf{buffer_.peek(), buffer_.readableBytes()};
                auto pos = buf.find(dashBoundaryCrlf_);
                // ignore everything before the first boundary
                if (pos == std::string::npos)
                {
                    buffer_.retrieve(buffer_.readableBytes() -
                                     dashBoundaryCrlf_.size() + 1);
                    return;
                }
                // found
                buffer_.retrieve(pos + dashBoundaryCrlf_.size());
                status_ = Status::kExpectNewEntry;
                continue;
            }
            case Status::kExpectNewEntry:
            {
                currentHeader_.name.clear();
                currentHeader_.filename.clear();
                currentHeader_.contentType.clear();
                status_ = Status::kExpectHeader;
                continue;
            }
            case Status::kExpectHeader:
            {
                std::string_view buf{buffer_.peek(), buffer_.readableBytes()};
                auto pos = buf.find(crlf_);
                if (pos == std::string::npos)
                {
                    // same magic number in HttpRequestParser::parseRequest()
                    if (buffer_.readableBytes() > 60 * 1024)
                    {
                        isValid_ = false;
                    }
                    return;  // header incomplete, wait for more data
                }
                // empty line
                if (pos == 0)
                {
                    buffer_.retrieve(crlf_.size());
                    status_ = Status::kExpectBody;
                    headerCb(currentHeader_);
                    continue;
                }
                // found header line
                // parseHeaderLine(buf.substr(0, pos));

                auto [keyView, valueView] =
                    parseLine(buf.data(), buf.data() + pos);
                if (keyView.empty() || valueView.empty())
                {
                    // Bad header
                    isValid_ = false;
                    return;
                }

                std::string key(keyView);
                std::transform(key.begin(),
                               key.end(),
                               key.begin(),
                               [](unsigned char c) { return tolower(c); });

                if (key == "content-type")
                {
                    currentHeader_.contentType = valueView;
                }
                else if (key == "content-disposition")
                {
                    static const std::string_view nameKey = "name=";
                    static const std::string_view fileNameKey = "filename=";

                    // Extract name
                    auto namePos = valueView.find(nameKey);
                    if (namePos == std::string::npos)
                    {
                        // name absent
                        isValid_ = false;
                        return;
                    }
                    namePos += nameKey.size();
                    size_t nameEnd;
                    if (valueView[namePos] == '"')
                    {
                        ++namePos;
                        nameEnd = valueView.find('"', namePos);
                    }
                    else
                    {
                        nameEnd = valueView.find(';', namePos);
                    }
                    if (nameEnd == std::string::npos)
                    {
                        // name end not found
                        isValid_ = false;
                        return;
                    }
                    currentHeader_.name =
                        valueView.substr(namePos, nameEnd - namePos);

                    // Extract filename
                    auto fileNamePos = valueView.find(fileNameKey, nameEnd);
                    if (fileNamePos != std::string::npos)
                    {
                        fileNamePos += fileNameKey.size();
                        size_t fileNameEnd;
                        if (valueView[fileNamePos] == '"')
                        {
                            ++fileNamePos;
                            fileNameEnd = valueView.find('"', fileNamePos);
                        }
                        else
                        {
                            fileNameEnd = valueView.find(';', fileNamePos);
                        }
                        currentHeader_.filename =
                            valueView.substr(fileNamePos,
                                             fileNameEnd - fileNamePos);
                    }
                }
                // ignore other headers
                buffer_.retrieve(pos + crlf_.size());
                continue;
            }
            case Status::kExpectBody:
            {
                if (buffer_.readableBytes() < crlfDashBoundary_.size())
                {
                    return;  // not enough data to check boundary
                }
                std::string_view buf(buffer_.peek(), buffer_.readableBytes());
                auto pos = buf.find(crlfDashBoundary_);
                if (pos == std::string::npos)
                {
                    // boundary not found, leave potential partial boundary
                    size_t len =
                        buffer_.readableBytes() - crlfDashBoundary_.size();
                    if (len > 0)
                    {
                        dataCb(buffer_.peek(), len);
                        buffer_.retrieve(len);
                    }
                    return;
                }
                // found boundary
                dataCb(buffer_.peek(), pos);
                buffer_.retrieve(pos + crlfDashBoundary_.size());
                status_ = Status::kExpectEndOrNewEntry;
                continue;
            }
            case Status::kExpectEndOrNewEntry:
            {
                if (buffer_.readableBytes() < crlf_.size())
                {
                    return;
                }
                // Check new entry
                std::string_view buf(buffer_.peek(), buffer_.readableBytes());
                if (startsWith(buf, crlf_))
                {
                    buffer_.retrieve(crlf_.size());
                    status_ = Status::kExpectNewEntry;
                    continue;
                }

                // Check end
                if (buffer_.readableBytes() < dash_.size())
                {
                    return;
                }
                if (startsWith(buf, dash_))
                {
                    isFinished_ = true;
                    buffer_.retrieveAll();  // ignore epilogue
                    return;
                }
                isValid_ = false;
                return;
            }
        }
    }
}
