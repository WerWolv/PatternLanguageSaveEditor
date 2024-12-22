import './App.css'

import Module from 'plwasm/plwasm.js'
import {useEffect} from "react";

let PatternLanguage;
let patternSourceCode = "";

Module().then((module) => {
    PatternLanguage = module;
    PatternLanguage._initialize();
});

function loadLocalFile() {
    let input = document.getElementById('input');
    let fileLabel = document.getElementById('fileLabel');

    input.onchange = e => {
        let file = e.target.files[0];

        let reader = new FileReader();

        reader.readAsArrayBuffer(file);
        fileLabel.innerText = file.name;

        reader.onload = readerEvent => {
            const uint8Arr = new Uint8Array(readerEvent.target.result);
            const num_bytes = uint8Arr.length * uint8Arr.BYTES_PER_ELEMENT;
            const data_ptr = PatternLanguage._malloc(num_bytes);
            const data_on_heap = new Uint8Array(PatternLanguage.HEAPU8.buffer, data_ptr, num_bytes);
            data_on_heap.set(uint8Arr);
            PatternLanguage._setData(data_on_heap.byteOffset, uint8Arr.length);
            PatternLanguage._free(data_ptr);

            execute();
        }
    };

    input.click();
}

function getUIConfig() {
    return PatternLanguage.UTF8ToString(PatternLanguage._getUIConfig());
}

function printToConsole(text, level) {
    let console = document.getElementById('console');

    let line = document.createElement('p');
    line.innerText = text;
    line.className = level;

    console.appendChild(line);
}

function clearConsole() {
    document.getElementById('console').innerText = '';
}

function getLogLevel(line) {
    if (line.startsWith("[DEBUG]"))
        return "debug";
    else if (line.startsWith("[INFO]"))
        return "info";
    else if (line.startsWith("[WARN]"))
        return "warning";
    else if (line.startsWith("[ERROR]"))
        return "error";
    else
        return "";
}

function setPatternSourceCode(sourceCode) {
    patternSourceCode = sourceCode;
}

function execute() {
    clearConsole();

    PatternLanguage._executePatternLanguageCode(PatternLanguage.allocateUTF8(patternSourceCode));
    let result = PatternLanguage.UTF8ToString(PatternLanguage._getConsoleResult())

    for (let line of result.split('\n\x01')) {
        printToConsole(line, getLogLevel(line));
    }

    console.log(getUIConfig());
}

function App() {
    let loaded = false;
    useEffect(() => {
        if (loaded) return;
        loaded = true;

        const urlParams = new URLSearchParams(window.location.search);
        if (urlParams.has("gist")) {
            let gist_id = urlParams.get("gist");
            fetch(`https://api.github.com/gists/${gist_id}`).then(response => response.json()).then(data => {
                let files = data.files;
                if (files === undefined) return

                let file = files[Object.keys(files)[0]];
                if (file === undefined) return

                let raw_url = file.raw_url;
                if (raw_url === undefined) return

                fetch(raw_url).then(response => response.text()).then((text) => {
                    console.log(`Loaded pattern source code from Gist '${gist_id}'!`)
                    setPatternSourceCode(text);
                })
            })
        } else if (urlParams.has("code")) {
            let encodedCode = urlParams.get("code");
            let decodedCode = atob(encodedCode.replace(/-/g, '+').replace(/_/g, '/').replace(/\s/g, ''));

            console.log(`Loaded pattern source code from URL '${encodedCode}'!`)
            setPatternSourceCode(decodedCode);
        }
    }, []);

    return (
        <div className="App">
            <div style={{ display: "flex", flexDirection: "row" }}>
                <div>
                  <div style={{ height: '75vh', width: '100vw' }} id={'editorContainer'}></div>
                  <div style={{ height: '20vh', width: 'calc(100vw - 2px)' }} className={'console'} id={'console'}></div>
                </div>
            </div>
            <div style={{ height: '5vh', width: '100vw',  display: "flex", flexDirection: "row" }} className={'buttons'}>
                <button onClick={loadLocalFile}>Load File</button>
                <label id={'fileLabel'}>No file selected</label>
                <input type="file" id="input" style={{ display: "none" }}/>
            </div>
        </div>
    )
}

export default App
