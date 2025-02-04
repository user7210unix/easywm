#!/bin/bash

# Function to display package names for manual installation
print_manual_install() {
    echo "Manual installation required. Install the following packages:"
    echo " - libX11-devel"
    echo " - libXft-devel"
    echo " - libXkbcommon-devel"
    echo " - libXrandr-devel"
    echo " - libXcomposite-devel"
    echo " - libXdamage-devel"
    echo " - xorg-xprop"
    echo " - gcc"
    echo " - make"
    echo " - xorg-server (or equivalent X server package)"
    echo " - procps (or equivalent)"
}

# Function to install dependencies on Ubuntu/Debian
install_ubuntu_debian() {
    echo "Installing dependencies for Ubuntu/Debian..."
    sudo apt update
    sudo apt install -y \
        libx11-dev \
        libxft-dev \
        libxkbcommon-dev \
        libxrandr-dev \
        libxcomposite-dev \
        libxdamage-dev \
        x11-utils \
        gcc \
        make \
        xorg \
        procps
}

# Function to install dependencies on Arch Linux
install_arch() {
    echo "Installing dependencies for Arch Linux..."
    sudo pacman -Syu --noconfirm
    sudo pacman -S --noconfirm \
        libx11 \
        libxft \
        libxkbcommon \
        libxrandr \
        libxcomposite \
        libxdamage \
        xorg-xprop \
        gcc \
        make \
        xorg-server \
        xorg-xinit \
        procps-ng
}

# Function to install dependencies on Fedora
install_fedora() {
    echo "Installing dependencies for Fedora..."
    sudo dnf install -y \
        libX11-devel \
        libXft-devel \
        libXkbcommon-devel \
        libXrandr-devel \
        libXcomposite-devel \
        libXdamage-devel \
        xorg-x11-utils \
        gcc \
        make \
        xorg-x11-server-utils \
        procps-ng
}

# Function to install dependencies on Gentoo
install_gentoo() {
    echo "Installing dependencies for Gentoo..."
    sudo emerge --ask x11-libs/libX11 x11-libs/libXft x11-libs/libXkbcommon x11-libs/libXrandr x11-libs/libXcomposite x11-libs/libXdamage x11-apps/xorg-xprop dev-lang/gcc sys-apps/procps
}

# Function to install dependencies on Void Linux
install_void() {
    echo "Installing dependencies for Void Linux..."
    sudo xbps-install -Syu
    sudo xbps-install -y \
        libX11-devel \
        libXft-devel \
        libXkbcommon-devel \
        libXrandr-devel \
        libXcomposite-devel \
        libXdamage-devel \
        xorg-xprop \
        gcc \
        make \
        procps
}

# Function to install dependencies based on distribution
install_dependencies() {
    if [[ -f /etc/os-release ]]; then
        # Check the distribution
        if grep -q -i "ubuntu\|debian" /etc/os-release; then
            install_ubuntu_debian
        elif grep -q -i "arch" /etc/os-release; then
            install_arch
        elif grep -q -i "fedora" /etc/os-release; then
            install_fedora
        elif grep -q -i "gentoo" /etc/os-release; then
            install_gentoo
        elif grep -q -i "void" /etc/os-release; then
            install_void
        else
            echo "Unknown distribution."
            print_manual_install
            exit 1
        fi
    else
        echo "Could not determine the distribution."
        print_manual_install
        exit 1
    fi
}

# Function to add exec easywm to .xinitrc
add_to_xinitrc() {
    echo "Adding 'exec easywm' to .xinitrc..."
    if ! grep -q "exec easywm" ~/.xinitrc; then
        echo "exec easywm" >> ~/.xinitrc
    else
        echo "'exec easywm' already exists in .xinitrc"
    fi
}

# Function to clone, build, and install easywm
install_easywm() {
    echo "Cloning the easywm repository..."
    git clone https://github.com/user7210unix/easywm.git
    cd easywm || exit
    echo "Building and installing easywm..."
    sudo make clean install
}

# Function to download and install Nerd Font
install_nerd_font() {
    echo "Installing Nerd Font (JetBrainsMono)..."
    mkdir -p ~/.fonts
    wget -O ~/.fonts/JetBrainsMono.zip https://github.com/ryanoasis/nerd-fonts/releases/download/v3.3.0/JetBrainsMono.zip
    unzip -o ~/.fonts/JetBrainsMono.zip -d ~/.fonts/
    rm ~/.fonts/JetBrainsMono.zip
    fc-cache -vf
}

# Main script execution
install_dependencies
add_to_xinitrc
install_easywm
install_nerd_font

# Clear the terminal and display completion message
clear
echo "Installation complete! You can now start the easywm window manager."
