// main.cpp
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono> // For timing operations
#include <memory> // For std::make_shared

// Crow includes
#include <crow.h>
#include <crow/multipart.h> // For multipart/form-data parsing

// Our project includes
#include "file_manager.hpp"
#include "chunk_config.hpp"
#include "file_metadata.hpp" // For metadata handling

namespace fs = std::filesystem;

// Helper to get Content-Type from filename (simplified)
std::string getContentType(const std::string& filename) {
    fs::path p(filename);
    std::string ext = p.extension().string();
    if (ext == ".txt") return "text/plain";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".bin") return "application/octet-stream";
    return "application/octet-stream"; // Default
}

int main() {
    // Determine optimal number of threads for the FileManager's thread pool
    const size_t num_fm_threads = std::thread::hardware_concurrency();
    // Default to a reasonable number if hardware_concurrency returns 0 or too few
    FileManager::FileManager fm(num_fm_threads == 0 ? 4 : num_fm_threads);

    // --- Crow Application Setup ---
    crow::SimpleApp app; // Create a Crow app instance

    // Shared pointer for FileManager instance to be captured by lambda routes
    // This allows the FileManager to persist across requests
    auto fm_ptr = std::make_shared<FileManager::FileManager>(num_fm_threads == 0 ? 4 : num_fm_threads);

    // --- Base URL & Port ---
    // The base URL will be http://localhost:8080/
    // Your Postman requests will target this.

    // --- POST /files: Upload a new file ---
    // Expects multipart/form-data with fields:
    // - file: the actual file content
    // - filename: (optional) original filename, if not provided in multipart-data
    // - content_type: (optional) content type, if not provided in multipart-data
    CROW_ROUTE(app, "/files").methods("POST"_method)
    ([fm_ptr](const crow::request& req) {
        if (!req.get_header("Content-Type").rfind("multipart/form-data", 0) == 0) {
            return crow::response(400, "Bad Request: Expected multipart/form-data.");
        }

        crow::multipart::message multipart_data(req);
        std::string original_filename_from_form;
        std::string content_type_from_form;
        crow::multipart::part* file_part = nullptr;

        for (const auto& part : multipart_data.parts) {
            if (part.get_name() == "file") {
                file_part = &part;
            } else if (part.get_name() == "filename") {
                original_filename_from_form = part.get_body();
            } else if (part.get_name() == "content_type") {
                content_type_from_form = part.get_body();
            }
        }

        if (!file_part) {
            return crow::response(400, "Bad Request: 'file' part missing in multipart/form-data.");
        }

        // Determine filename: Use form field if provided, else from multipart part, else generate.
        std::string filename_to_use;
        if (!original_filename_from_form.empty()) {
            filename_to_use = original_filename_from_form;
        } else if (!file_part->get_filename().empty()) {
            filename_to_use = file_part->get_filename();
        } else {
            filename_to_use = "uploaded_file_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        }

        // Determine content type: Use form field if provided, else from multipart part, else guess
        std::string content_type_to_use;
        if (!content_type_from_form.empty()) {
            content_type_to_use = content_type_from_form;
        } else if (!file_part->get_header("Content-Type").empty()) {
            content_type_to_use = file_part->get_header("Content-Type");
        } else {
            content_type_to_use = getContentType(filename_to_use);
        }

        // Save the uploaded file temporarily
        fs::path temp_filepath = fs::temp_directory_path() / filename_to_use;
        std::ofstream temp_ofs(temp_filepath, std::ios::binary);
        if (!temp_ofs.is_open()) {
            return crow::response(500, "Internal Server Error: Could not create temporary file.");
        }
        temp_ofs.write(file_part->get_body().data(), file_part->get_body().size());
        temp_ofs.close();

        try {
            // Call FileManager to upload the file
            FileManager::Metadata::FileMetadata metadata = fm_ptr->uploadFile(temp_filepath.string(), filename_to_use, content_type_to_use);

            // Delete temporary file
            fs::remove(temp_filepath);

            crow::json::wvalue response_json;
            response_json["filename"] = metadata.original_filename;
            response_json["size"] = metadata.file_size_bytes;
            response_json["content_type"] = metadata.content_type;
            response_json["created_at"] = metadata.created_at;
            response_json["chunk_cids"] = crow::json::wvalue::list(metadata.chunk_cids.begin(), metadata.chunk_cids.end());

            return crow::response(201, response_json); // 201 Created
        } catch (const std::exception& e) {
            std::cerr << "Error during file upload: " << e.what() << std::endl;
            fs::remove(temp_filepath); // Ensure temp file is cleaned up on error
            return crow::response(500, "Internal Server Error: " + std::string(e.what()));
        }
    });

    // --- GET /files/<filename>: Retrieve a file ---
    CROW_ROUTE(app, "/files/<string>")
    ([fm_ptr](const crow::request& req, std::string filename) {
        fs::path temp_output_path = fs::temp_directory_path() / ("retrieved_" + filename);

        try {
            if (fm_ptr->retrieveFile(filename, temp_output_path.string())) {
                std::ifstream ifs(temp_output_path, std::ios::binary);
                if (!ifs.is_open()) {
                    return crow::response(500, "Internal Server Error: Could not open retrieved file.");
                }

                // Read file into memory to send as response
                std::vector<char> buffer((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                ifs.close();

                // Delete temporary file
                fs::remove(temp_output_path);

                crow::response res(200);
                res.set_header("Content-Type", getContentType(filename));
                res.set_header("Content-Disposition", "attachment; filename=\"" + filename + "\"");
                res.write(std::string(buffer.begin(), buffer.end()));
                return res;
            } else {
                return crow::response(404, "File not found or retrieval failed.");
            }
        } catch (const std::runtime_error& e) {
            std::cerr << "Error retrieving file: " << e.what() << std::endl;
            fs::remove(temp_output_path); // Clean up temp file on error
            if (std::string(e.what()).find("not found") != std::string::npos) {
                return crow::response(404, "File not found.");
            }
            return crow::response(500, "Internal Server Error: " + std::string(e.what()));
        }
    });

    // --- GET /chunks/<hash>: Retrieve a specific chunk ---
    CROW_ROUTE(app, "/chunks/<string>")
    ([fm_ptr](const crow::request& req, std::string chunk_hash) {
        try {
            std::vector<char> chunk_data = fm_ptr->retrieveChunk(chunk_hash);

            crow::response res(200);
            res.set_header("Content-Type", "application/octet-stream"); // Generic for binary data
            res.set_header("Content-Disposition", "attachment; filename=\"" + chunk_hash + ".chunk\"");
            res.write(std::string(chunk_data.begin(), chunk_data.end()));
            return res;
        } catch (const std::runtime_error& e) {
            std::cerr << "Error retrieving chunk: " << e.what() << std::endl;
            if (std::string(e.what()).find("not found") != std::string::npos) {
                return crow::response(404, "Chunk not found.");
            }
            return crow::response(500, "Internal Server Error: " + std::string(e.what()));
        }
    });

    // --- DELETE /files/<filename>: Delete a file ---
    CROW_ROUTE(app, "/files/<string>").methods("DELETE"_method)
    ([fm_ptr](const crow::request& req, std::string filename) {
        try {
            if (fm_ptr->deleteFile(filename)) {
                return crow::response(204); // 204 No Content on successful deletion
            } else {
                return crow::response(404, "File not found or deletion failed."); // Might be due to file not existing
            }
        } catch (const std::exception& e) {
            std::cerr << "Error deleting file: " << e.what() << std::endl;
            if (std::string(e.what()).find("not found") != std::string::npos) {
                return crow::response(404, "File not found.");
            }
            return crow::response(500, "Internal Server Error: " + std::string(e.what()));
        }
    });

    // --- PUT /files/<filename>: Update a file ---
    // Expects multipart/form-data similar to POST /files
    CROW_ROUTE(app, "/files/<string>").methods("PUT"_method)
    ([fm_ptr](const crow::request& req, std::string filename_to_update) {
        if (!req.get_header("Content-Type").rfind("multipart/form-data", 0) == 0) {
            return crow::response(400, "Bad Request: Expected multipart/form-data.");
        }

        crow::multipart::message multipart_data(req);
        std::string content_type_from_form;
        crow::multipart::part* file_part = nullptr;

        for (const auto& part : multipart_data.parts) {
            if (part.get_name() == "file") {
                file_part = &part;
            } else if (part.get_name() == "content_type") {
                content_type_from_form = part.get_body();
            }
        }

        if (!file_part) {
            return crow::response(400, "Bad Request: 'file' part missing in multipart/form-data for update.");
        }

        // Determine content type: Use form field if provided, else from multipart part, else guess
        std::string content_type_to_use;
        if (!content_type_from_form.empty()) {
            content_type_to_use = content_type_from_form;
        } else if (!file_part->get_header("Content-Type").empty()) {
            content_type_to_use = file_part->get_header("Content-Type");
        } else {
            content_type_to_use = getContentType(filename_to_update); // Guess based on original filename if content-type not given
        }

        // Save the updated file temporarily
        fs::path temp_filepath = fs::temp_directory_path() / ("updated_" + filename_to_update);
        std::ofstream temp_ofs(temp_filepath, std::ios::binary);
        if (!temp_ofs.is_open()) {
            return crow::response(500, "Internal Server Error: Could not create temporary file for update.");
        }
        temp_ofs.write(file_part->get_body().data(), file_part->get_body().size());
        temp_ofs.close();

        try {
            // Call FileManager to update the file
            FileManager::Metadata::FileMetadata updated_metadata = fm_ptr->updateFile(filename_to_update, temp_filepath.string(), content_type_to_use);

            // Delete temporary file
            fs::remove(temp_filepath);

            crow::json::wvalue response_json;
            response_json["filename"] = updated_metadata.original_filename;
            response_json["size"] = updated_metadata.file_size_bytes;
            response_json["content_type"] = updated_metadata.content_type;
            response_json["created_at"] = updated_metadata.created_at;
            response_json["chunk_cids"] = crow::json::wvalue::list(updated_metadata.chunk_cids.begin(), updated_metadata.chunk_cids.end());

            return crow::response(200, response_json); // 200 OK for update
        } catch (const std::exception& e) {
            std::cerr << "Error during file update: " << e.what() << std::endl;
            fs::remove(temp_filepath); // Ensure temp file is cleaned up on error
            if (std::string(e.what()).find("not found") != std::string::npos) {
                return crow::response(404, "File to update not found.");
            }
            return crow::response(500, "Internal Server Error: " + std::string(e.what()));
        }
    });


    // Start the Crow server on port 8080
    // Crow can bind to multiple ports or interfaces.
    // For local testing, 8080 is common.
    std::cout << "Starting File Manager Service on http://localhost:8080" << std::endl;
    app.port(8080).multithreaded().run(); // .multithreaded() makes Crow use multiple threads to handle requests

    return 0;
}