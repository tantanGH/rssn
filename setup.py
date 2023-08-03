import setuptools

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setuptools.setup(
    name="rssnd",
    version="0.6.0",
    author="tantanGH",
    author_email="tantanGH@github",
    license='MIT',
    description="backend service daemon for rssn over UART",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/tantanGH/rssnd",
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ],
    entry_points={
        'console_scripts': [
            'rssnd=rssnd.rssnd:main'
        ]
    },
    packages=setuptools.find_packages(),
    python_requires=">=3.7",
    setup_requires=["setuptools"],
    install_requires=["pyserial", "feedparser", "requests", "beautifulsoup4"], #"selenium"],
)
