#include "mini_judge/reporter.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mini_judge {
namespace {

std::string EscapeJson(const std::string& value) {
    std::string out;
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : value) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out.push_back(hex[c >> 4]);
                    out.push_back(hex[c & 0x0f]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

std::string EscapeCsv(const std::string& value) {
    if (value.find_first_of(",\"\n\r") == std::string::npos) {
        return value;
    }
    std::string out = "\"";
    for (char c : value) {
        if (c == '"') {
            out += "\"\"";
        } else {
            out.push_back(c);
        }
    }
    out += "\"";
    return out;
}

std::string EscapeHtml(const std::string& value) {
    std::string out;
    for (unsigned char c : value) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&#39;";
                break;
            default:
                if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
                    out += "&#xfffd;";
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

std::string StatusClass(const CaseStatus status) {
    switch (status) {
        case CaseStatus::AC:
            return "status-ac";
        case CaseStatus::WA:
            return "status-wa";
        case CaseStatus::TLE:
            return "status-tle";
        case CaseStatus::RE:
            return "status-re";
        case CaseStatus::OLE:
            return "status-ole";
        case CaseStatus::SE:
            return "status-se";
    }
    return "status-se";
}

std::ofstream OpenReport(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open report file: " + path.u8string());
    }
    out.exceptions(std::ios::badbit | std::ios::failbit);
    return out;
}

void FinishReport(std::ofstream& out) {
    out.flush();
    out.close();
}

}  // namespace

void WriteJsonReport(const BatchReport& report, const std::filesystem::path& path) {
    std::ofstream out = OpenReport(path);
    out << "{\n";
    out << "  \"problem_id\": \"" << EscapeJson(report.problem_id) << "\",\n";
    out << "  \"generated_at\": \"" << EscapeJson(report.generated_at) << "\",\n";
    out << "  \"jobs\": " << report.jobs << ",\n";
    out << "  \"total_programs\": " << report.total_programs << ",\n";
    out << "  \"total_cases\": " << report.total_cases << ",\n";
    out << "  \"programs\": [\n";
    for (std::size_t i = 0; i < report.programs.size(); ++i) {
        const auto& program = report.programs[i];
        out << "    {\n";
        out << "      \"program_id\": \"" << EscapeJson(program.program_id) << "\",\n";
        out << "      \"total_score\": " << program.total_score << ",\n";
        out << "      \"max_score\": " << program.max_score << ",\n";
        out << "      \"passed_count\": " << program.passed_count << ",\n";
        out << "      \"failed_count\": " << program.failed_count << ",\n";
        out << "      \"ac_count\": " << program.ac_count << ",\n";
        out << "      \"wa_count\": " << program.wa_count << ",\n";
        out << "      \"tle_count\": " << program.tle_count << ",\n";
        out << "      \"re_count\": " << program.re_count << ",\n";
        out << "      \"ole_count\": " << program.ole_count << ",\n";
        out << "      \"se_count\": " << program.se_count << ",\n";
        out << "      \"cases\": [\n";
        for (std::size_t j = 0; j < program.cases.size(); ++j) {
            const auto& item = program.cases[j];
            out << "        {\n";
            out << "          \"test_id\": \"" << EscapeJson(item.test_id) << "\",\n";
            out << "          \"status\": \"" << ToString(item.status) << "\",\n";
            out << "          \"score\": " << item.score << ",\n";
            out << "          \"max_score\": " << item.max_score << ",\n";
            out << "          \"time_ms\": " << item.time_ms << ",\n";
            out << "          \"exit_code\": " << item.exit_code << ",\n";
            out << "          \"stdout_size\": " << item.stdout_size << ",\n";
            out << "          \"message\": \"" << EscapeJson(item.message) << "\"\n";
            out << "        }" << (j + 1 < program.cases.size() ? "," : "") << "\n";
        }
        out << "      ]\n";
        out << "    }" << (i + 1 < report.programs.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    FinishReport(out);
}

void WriteCsvSummary(const BatchReport& report, const std::filesystem::path& path) {
    std::ofstream out = OpenReport(path);
    out << "program_id,total_score,max_score,passed_count,failed_count,AC,WA,TLE,RE,OLE,SE\n";
    for (const auto& program : report.programs) {
        out << EscapeCsv(program.program_id) << ","
            << program.total_score << ","
            << program.max_score << ","
            << program.passed_count << ","
            << program.failed_count << ","
            << program.ac_count << ","
            << program.wa_count << ","
            << program.tle_count << ","
            << program.re_count << ","
            << program.ole_count << ","
            << program.se_count << "\n";
    }
    FinishReport(out);
}

void WriteHtmlReport(const BatchReport& report, const std::filesystem::path& path) {
    std::ofstream out = OpenReport(path);
    out << "<!DOCTYPE html>\n"
        << "<html lang=\"en\">\n"
        << "<head>\n"
        << "  <meta charset=\"utf-8\">\n"
        << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        << "  <title>mini-judge-core report</title>\n"
        << "  <style>\n"
        << "    :root { color-scheme: light; --bg:#f5f1e8; --panel:#fffdf8; --ink:#1f1a14; --muted:#6a6055; --line:#d9cdbf; --accent:#0c6c61; }\n"
        << "    body { margin:0; background:linear-gradient(160deg,#f1eadb,#fbf8f1); color:var(--ink); font-family:Georgia,'Times New Roman',serif; }\n"
        << "    .page { max-width:1100px; margin:0 auto; padding:32px 24px 48px; }\n"
        << "    .hero { background:var(--panel); border:1px solid var(--line); border-radius:20px; padding:24px; box-shadow:0 14px 40px rgba(31,26,20,.08); }\n"
        << "    h1,h2,h3 { margin:0 0 12px; }\n"
        << "    .meta { color:var(--muted); display:flex; gap:18px; flex-wrap:wrap; font-size:14px; }\n"
        << "    .summary { display:grid; grid-template-columns:repeat(auto-fit,minmax(180px,1fr)); gap:14px; margin-top:22px; }\n"
        << "    .card { background:var(--panel); border:1px solid var(--line); border-radius:18px; padding:18px; }\n"
        << "    .card strong { display:block; font-size:28px; margin-bottom:6px; }\n"
        << "    table { width:100%; border-collapse:collapse; margin-top:14px; background:var(--panel); border:1px solid var(--line); border-radius:18px; overflow:hidden; }\n"
        << "    th,td { padding:12px 14px; text-align:left; border-bottom:1px solid var(--line); vertical-align:top; }\n"
        << "    th { font-size:13px; letter-spacing:.04em; text-transform:uppercase; color:var(--muted); }\n"
        << "    tr:last-child td { border-bottom:none; }\n"
        << "    .program { margin-top:28px; }\n"
        << "    .pill { display:inline-block; padding:4px 10px; border-radius:999px; font-size:12px; font-weight:700; letter-spacing:.04em; }\n"
        << "    .status-ac { background:#dff3e7; color:#17603a; }\n"
        << "    .status-wa { background:#fae3d8; color:#8a3b17; }\n"
        << "    .status-tle { background:#f7e6b8; color:#7b5b00; }\n"
        << "    .status-re { background:#f7d9d9; color:#8b1e1e; }\n"
        << "    .status-ole { background:#eadcf8; color:#5e2f8a; }\n"
        << "    .status-se { background:#e2e5ea; color:#38404a; }\n"
        << "    .message { color:var(--muted); max-width:320px; }\n"
        << "    .footer { color:var(--muted); margin-top:28px; font-size:13px; }\n"
        << "  </style>\n"
        << "</head>\n"
        << "<body>\n"
        << "<div class=\"page\">\n"
        << "  <section class=\"hero\">\n"
        << "    <h1>mini-judge-core report</h1>\n"
        << "    <div class=\"meta\">\n"
        << "      <span>Problem: " << EscapeHtml(report.problem_id) << "</span>\n"
        << "      <span>Generated: " << EscapeHtml(report.generated_at) << "</span>\n"
        << "      <span>Jobs: " << report.jobs << "</span>\n"
        << "      <span>Programs: " << report.total_programs << "</span>\n"
        << "      <span>Cases: " << report.total_cases << "</span>\n"
        << "    </div>\n"
        << "  </section>\n"
        << "  <section class=\"summary\">\n";

    int total_ac = 0;
    int total_wa = 0;
    int total_tle = 0;
    int total_re = 0;
    int total_ole = 0;
    int total_se = 0;
    for (const auto& program : report.programs) {
        total_ac += program.ac_count;
        total_wa += program.wa_count;
        total_tle += program.tle_count;
        total_re += program.re_count;
        total_ole += program.ole_count;
        total_se += program.se_count;
    }

    out << "    <div class=\"card\"><strong>" << report.total_programs << "</strong><span>Programs</span></div>\n"
        << "    <div class=\"card\"><strong>" << report.total_cases << "</strong><span>Total cases</span></div>\n"
        << "    <div class=\"card\"><strong>" << total_ac << "</strong><span>Accepted cases</span></div>\n"
        << "    <div class=\"card\"><strong>" << total_wa << "</strong><span>Wrong answers</span></div>\n"
        << "    <div class=\"card\"><strong>" << total_tle << "</strong><span>Time limit exceeded</span></div>\n"
        << "    <div class=\"card\"><strong>" << (total_re + total_ole + total_se) << "</strong><span>Runtime/system failures</span></div>\n"
        << "  </section>\n";

    out << "  <section class=\"program\">\n"
        << "    <h2>Program summary</h2>\n"
        << "    <table>\n"
        << "      <thead><tr><th>Program</th><th>Score</th><th>Passed</th><th>AC</th><th>WA</th><th>TLE</th><th>RE</th><th>OLE</th><th>SE</th></tr></thead>\n"
        << "      <tbody>\n";
    for (const auto& program : report.programs) {
        out << "        <tr><td>" << EscapeHtml(program.program_id) << "</td><td>"
            << program.total_score << " / " << program.max_score << "</td><td>"
            << program.passed_count << " / " << program.cases.size() << "</td><td>"
            << program.ac_count << "</td><td>" << program.wa_count << "</td><td>"
            << program.tle_count << "</td><td>" << program.re_count << "</td><td>"
            << program.ole_count << "</td><td>" << program.se_count << "</td></tr>\n";
    }
    out << "      </tbody>\n"
        << "    </table>\n"
        << "  </section>\n";

    for (const auto& program : report.programs) {
        out << "  <section class=\"program\">\n"
            << "    <h2>Program " << EscapeHtml(program.program_id) << "</h2>\n"
            << "    <table>\n"
            << "      <thead><tr><th>Test</th><th>Status</th><th>Score</th><th>Time</th><th>Exit code</th><th>Output size</th><th>Message</th></tr></thead>\n"
            << "      <tbody>\n";
        for (const auto& item : program.cases) {
            out << "        <tr><td>" << EscapeHtml(item.test_id) << "</td><td><span class=\"pill "
                << StatusClass(item.status) << "\">" << ToString(item.status) << "</span></td><td>"
                << item.score << " / " << item.max_score << "</td><td>"
                << item.time_ms << " ms</td><td>" << item.exit_code << "</td><td>"
                << item.stdout_size << " B</td><td class=\"message\">" << EscapeHtml(item.message) << "</td></tr>\n";
        }
        out << "      </tbody>\n"
            << "    </table>\n"
            << "  </section>\n";
    }

    out << "  <div class=\"footer\">Generated by mini-judge-core. This report is meant for demo and showcase use, not production deployment.</div>\n"
        << "</div>\n"
        << "</body>\n"
        << "</html>\n";
    FinishReport(out);
}

}  // namespace mini_judge
