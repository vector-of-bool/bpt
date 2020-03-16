Vagrant.configure("2") do |config|
  # Refer: https://docs.vagrantup.com.

  config.vm.synced_folder ".", "/vagrant",
    type: 'rsync',
    rsync__exclude: ['_build/', '.mypy_cache/', '.pytest_cache/', '_prebuilt/']

  config.vm.define 'freebsd11' do |freebsd11|
    freebsd11.vm.box = 'generic/freebsd11'
    freebsd11.vm.provision 'shell', inline: <<-SHELL
      set -eu
      for package in python37 py37-pip ccache gcc9 gcc8 git; do
        echo "Installing $package"
        pkg install -qy $package
      done
      ln -fs g++9 /usr/local/bin/g++-9
      ln -fs gcc9 /usr/local/bin/gcc-9
      ln -fs g++8 /usr/local/bin/g++-8
      ln -fs gcc8 /usr/local/bin/gcc-8
      sudo -u vagrant pip install -q --user pytest pytest-xdist
    SHELL
  end

  config.vm.provider 'virtualbox' do |vbox|
    vbox.memory = 1024 * 4
    vbox.cpus = 8
  end
end
