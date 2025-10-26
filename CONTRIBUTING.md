# Contributing to Ubuntu Blockchain

Thank you for your interest in contributing to Ubuntu Blockchain! This document provides guidelines and instructions for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How Can I Contribute?](#how-can-i-contribute)
- [Development Setup](#development-setup)
- [Coding Standards](#coding-standards)
- [Pull Request Process](#pull-request-process)
- [Testing Guidelines](#testing-guidelines)
- [Documentation](#documentation)
- [Community](#community)

---

## Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inclusive environment for all contributors, regardless of:

- Experience level
- Gender identity and expression
- Sexual orientation
- Disability
- Personal appearance
- Race or ethnicity
- Age
- Religion or lack thereof

### Expected Behavior

- **Be Respectful**: Treat everyone with respect and kindness
- **Be Collaborative**: Work together constructively
- **Be Professional**: Focus on what is best for the project
- **Be Patient**: Help others learn and grow
- **Give Credit**: Acknowledge others' contributions

### Unacceptable Behavior

- Harassment, discrimination, or offensive comments
- Trolling, insulting, or derogatory remarks
- Publishing others' private information
- Any conduct that could be reasonably considered inappropriate

### Reporting

Report violations to: conduct@ubuntublockchain.xyz

---

## How Can I Contribute?

### Reporting Bugs

Before submitting a bug report:

1. **Check existing issues** to avoid duplicates
2. **Verify** you're using the latest version
3. **Gather information**:
   - Ubuntu Blockchain version
   - Operating system and version
   - Steps to reproduce
   - Expected vs actual behavior
   - Relevant logs or screenshots

**Create an issue with**:

```markdown
## Bug Description
[Clear description of the bug]

## Steps to Reproduce
1. Step one
2. Step two
3. ...

## Expected Behavior
[What should happen]

## Actual Behavior
[What actually happens]

## Environment
- OS: Ubuntu 22.04
- UBU Version: 1.0.0
- Build Type: Release

## Additional Context
[Logs, screenshots, etc.]
```

### Suggesting Features

Feature requests should be:

- **Clearly described**: What problem does it solve?
- **Well-scoped**: Specific and achievable
- **Aligned**: Fits project goals
- **Researched**: Consider alternatives

**Create an issue with**:

```markdown
## Feature Description
[Clear description of the feature]

## Problem Statement
[What problem does this solve?]

## Proposed Solution
[How would it work?]

## Alternatives Considered
[Other approaches]

## Additional Context
[Mockups, examples, etc.]
```

### Contributing Code

Contributions are welcome for:

- Bug fixes
- New features
- Performance improvements
- Documentation improvements
- Test coverage
- Code cleanup

---

## Development Setup

### Prerequisites

See [BUILD.md](BUILD.md) for detailed build instructions.

**Quick Setup (Ubuntu)**:

```bash
# Install dependencies
sudo apt install -y build-essential cmake git \
    libssl-dev librocksdb-dev libboost-all-dev \
    libspdlog-dev nlohmann-json3-dev libprotobuf-dev \
    libgtest-dev libbenchmark-dev

# Clone repository
git clone https://github.com/EmekaIwuagwu/ubuntu-blockchain.git
cd ubuntu-blockchain

# Create build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZERS=ON
cmake --build . -j$(nproc)

# Run tests
ctest --output-on-failure
```

### Development Tools

**Recommended**:

- **IDE**: VSCode, CLion, or Visual Studio
- **Linter**: clang-tidy
- **Formatter**: clang-format
- **Static Analyzer**: cppcheck
- **Debugger**: gdb or lldb
- **Profiler**: perf, Valgrind, or instruments

### IDE Configuration

**VSCode** (.vscode/settings.json):

```json
{
  "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
  "cmake.configureArgs": [
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DUSE_SANITIZERS=ON"
  ],
  "editor.formatOnSave": true,
  "C_Cpp.clang_format_path": "/usr/bin/clang-format"
}
```

---

## Coding Standards

### C++ Style Guide

We follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with minor modifications.

#### Key Points

**Naming Conventions**:

```cpp
// Classes: PascalCase
class BlockHeader { };

// Functions: camelCase
void validateBlock();

// Variables: camelCase
int blockHeight;

// Constants: UPPER_SNAKE_CASE
const int MAX_BLOCK_SIZE = 1000000;

// Namespaces: lowercase
namespace ubuntu::core { }

// Private members: m_prefix
class Wallet {
private:
    std::string m_password;
};
```

**File Organization**:

```cpp
// header.h
#pragma once  // Prefer over include guards

#include "ubuntu/core/block.h"  // Project headers first
#include <vector>               // Then system headers

namespace ubuntu::core {

class MyClass {
public:
    MyClass();
    ~MyClass();

    void publicMethod();

private:
    void privateMethod();
    int m_memberVariable;
};

}  // namespace ubuntu::core
```

**Code Formatting**:

```cpp
// Use clang-format with provided .clang-format file
// 4 spaces for indentation
// 100 character line limit
// Braces on same line for functions

void function(int param) {
    if (condition) {
        doSomething();
    } else {
        doSomethingElse();
    }
}
```

**Modern C++ Features**:

```cpp
// Use C++20 features
auto value = getValue();  // Type inference
std::unique_ptr<Object> ptr;  // Smart pointers
std::vector<int> vec{1, 2, 3};  // Uniform initialization
constexpr int SIZE = 100;  // Compile-time constants
```

**Error Handling**:

```cpp
// Use exceptions for exceptional cases
if (!file.open()) {
    throw std::runtime_error("Failed to open file");
}

// Use std::optional for optional values
std::optional<Block> getBlock(const Hash256& hash);

// Use std::expected (C++23) or Result type
Result<Transaction> parseTransaction(const std::vector<uint8_t>& data);
```

**Documentation**:

```cpp
/**
 * @brief Validates a block according to consensus rules
 *
 * Performs comprehensive validation including:
 * - Proof of work verification
 * - Merkle root validation
 * - Transaction validation
 *
 * @param block The block to validate
 * @param chainParams Chain parameters for validation
 * @return true if block is valid, false otherwise
 * @throws std::runtime_error if critical validation error occurs
 */
bool validateBlock(const Block& block, const ChainParams& chainParams);
```

### Code Quality

**Static Analysis**:

```bash
# Run clang-tidy
clang-tidy src/**/*.cpp -- -std=c++20 -Iinclude

# Run cppcheck
cppcheck --enable=all --inconclusive --std=c++20 src/
```

**Code Formatting**:

```bash
# Format all source files
find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Check formatting
git diff --exit-code
```

**Linting**:

```bash
# Check for common issues
./scripts/lint.sh
```

---

## Pull Request Process

### Before Submitting

1. **Create an issue first** for significant changes
2. **Fork the repository**
3. **Create a feature branch**: `git checkout -b feature/my-feature`
4. **Make your changes**
5. **Add tests** for new functionality
6. **Run tests**: `ctest`
7. **Update documentation**
8. **Format code**: `clang-format -i <files>`
9. **Commit with clear messages**

### Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types**:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting)
- `refactor`: Code refactoring
- `test`: Adding or updating tests
- `perf`: Performance improvements
- `chore`: Build process or auxiliary tool changes

**Examples**:

```
feat(wallet): Add HD wallet support with BIP-44

Implement hierarchical deterministic wallet using BIP-32/39/44.
- Generate mnemonic seeds
- Derive keys from path m/44'/9999'/0'/0/0
- Support both legacy and bech32 addresses

Closes #123
```

```
fix(consensus): Correct difficulty adjustment calculation

The previous implementation incorrectly calculated target based on
average time instead of median time, leading to incorrect difficulty.

Fixes #456
```

### Submitting Pull Request

1. **Push to your fork**: `git push origin feature/my-feature`
2. **Create Pull Request** on GitHub
3. **Fill out PR template**:

```markdown
## Description
[What does this PR do?]

## Motivation
[Why is this change needed?]

## Changes
- Change 1
- Change 2

## Testing
[How was this tested?]

## Checklist
- [ ] Tests added/updated
- [ ] Documentation updated
- [ ] Code formatted
- [ ] Commits follow convention
- [ ] No merge conflicts
```

4. **Address review feedback**
5. **Wait for approval** from 2+ reviewers
6. **Squash and merge** when approved

### Review Process

**Reviewers will check**:

- Code quality and style
- Test coverage
- Documentation
- Performance impact
- Security implications
- Breaking changes

**Response expectations**:

- Initial review: Within 3 business days
- Follow-up: Within 1 business day
- Approval: Requires 2+ approvals

---

## Testing Guidelines

### Test Coverage

All new code must include tests:

- **Unit Tests**: Test individual functions/classes
- **Integration Tests**: Test component interactions
- **End-to-End Tests**: Test complete workflows

**Minimum Coverage**: 80%

### Writing Tests

**Unit Test Example**:

```cpp
#include <gtest/gtest.h>
#include "ubuntu/core/transaction.h"

TEST(TransactionTest, ValidateInputs) {
    Transaction tx;
    // ... setup transaction ...

    EXPECT_TRUE(tx.validateInputs());
}

TEST(TransactionTest, RejectInvalidSignature) {
    Transaction tx;
    // ... create tx with invalid signature ...

    EXPECT_FALSE(tx.validateSignatures());
}
```

**Integration Test Example**:

```cpp
TEST(BlockchainTest, AcceptValidBlock) {
    Blockchain chain;
    Block block = createValidBlock();

    EXPECT_TRUE(chain.acceptBlock(block));
    EXPECT_EQ(chain.getHeight(), 1);
}
```

### Running Tests

```bash
# Run all tests
ctest --output-on-failure

# Run specific test
ctest -R TransactionTest --verbose

# Run with coverage
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=ON
cmake --build .
ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

### Benchmarking

```bash
# Run benchmarks
./bin/ubu_bench

# Run specific benchmark
./bin/ubu_bench --benchmark_filter=BM_SHA256

# Compare performance
./bin/ubu_bench --benchmark_out=before.json
# ... make changes ...
./bin/ubu_bench --benchmark_out=after.json
./scripts/compare_bench.py before.json after.json
```

---

## Documentation

### Code Documentation

- **Public APIs**: Must have Doxygen comments
- **Complex logic**: Explain the "why"
- **TODOs**: Include issue reference

```cpp
/**
 * @brief Validates proof of work for a block header
 *
 * Checks that the block hash meets the target difficulty specified
 * in the bits field. Uses double-SHA256 for hashing.
 *
 * @param header Block header to validate
 * @param params Chain parameters containing consensus rules
 * @return true if proof of work is valid
 *
 * @see PoW::getTargetFromBits()
 *
 * @note This is computationally expensive for high difficulties
 */
bool checkProofOfWork(const BlockHeader& header, const ChainParams& params);
```

### User Documentation

Update relevant docs when changing:

- **README.md**: Overview and quick start
- **BUILD.md**: Build instructions
- **docs/API.md**: RPC API reference
- **docs/DEPLOYMENT.md**: Deployment guide

### Changelog

Update CHANGELOG.md for all changes:

```markdown
## [1.1.0] - 2023-11-15

### Added
- HD wallet support (BIP-32/39/44)
- Bech32 address encoding

### Changed
- Improved block validation performance by 25%

### Fixed
- Memory leak in UTXO cache

### Security
- Fixed RPC authentication bypass
```

---

## Community

### Communication Channels

- **GitHub Issues**: Bug reports, feature requests
- **GitHub Discussions**: General questions, ideas
- **Discord**: https://discord.gg/ubuntu-blockchain
- **Twitter**: @UbuntuBlockchain
- **Email**: dev@ubuntublockchain.xyz

### Getting Help

- Check existing documentation
- Search GitHub issues
- Ask in Discord
- Open a GitHub discussion

### Recognition

Contributors are recognized in:

- CONTRIBUTORS.md
- Release notes
- Project website

---

## Development Roadmap

See [ROADMAP.md](ROADMAP.md) for planned features and timeline.

**Current Priorities**:

1. Complete test coverage
2. Performance optimization
3. Multi-signature support
4. Lightning Network integration

---

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

See [LICENSE](LICENSE) for details.

---

## Questions?

Don't hesitate to ask! We're here to help:

- **Discord**: https://discord.gg/ubuntu-blockchain
- **Email**: dev@ubuntublockchain.xyz
- **GitHub Discussions**: https://github.com/EmekaIwuagwu/ubuntu-blockchain/discussions

Thank you for contributing to Ubuntu Blockchain!
